#include "FabServer/AIProviders/Local/UmgMcpLiteRtLmAiProvider.h"
#include "UmgMcp.h"
#include "Engine/Engine.h"
// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "FabServer/ChatSystem/UmgMcpActiveMessageSubsystem.h"
#include "FabServer/ChatSystem/UmgMcpSessionManagerSubsystem.h"
#include "FabServer/Agent/UmgMcpAgent.h"
#include "LiteRtLmUnrealApi.h"
#include "LiteRtLmSubsystem.h"
#include "Engine/Engine.h"
#include "Async/Async.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Widgets/Text/STextBlock.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#define LOCTEXT_NAMESPACE "FUmgMcpLiteRtLmAiProvider"

// ============================================================
// Lifecycle
// ============================================================

FUmgMcpLiteRtLmAiProvider::FUmgMcpLiteRtLmAiProvider(const FString& InModelFile)
    : ModelFile(InModelFile)
{
}

FUmgMcpLiteRtLmAiProvider::~FUmgMcpLiteRtLmAiProvider()
{
}

void FUmgMcpLiteRtLmAiProvider::Stop()
{
    bStopRequested = true;
}

void FUmgMcpLiteRtLmAiProvider::UnloadModel()
{
    UE_LOG(LogTemp, Log, TEXT("[LiteRtLmProvider] UnloadModel called."));

    FLiteRtLmUnrealApi::UnloadModel();
}

// ============================================================
// EnsureModelLoadedAsync
// ============================================================

void FUmgMcpLiteRtLmAiProvider::EnsureModelLoadedAsync(TFunction<void(bool)> OnComplete)
{
    ULiteRtLmSubsystem* LiteRtLm = GEngine ? GEngine->GetEngineSubsystem<ULiteRtLmSubsystem>() : nullptr;
    if (!LiteRtLm)
    {
        if (OnComplete) OnComplete(false);
        return;
    }

    // Resolve model path (relative → Content/Models directory)
    FString AbsPath = ModelFile;
    if (FPaths::IsRelative(ModelFile))
    {
        AbsPath = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Models"), ModelFile));
    }

    if (!FPaths::FileExists(AbsPath))
    {
        UE_LOG(LogUmgMcp, Error, TEXT("[LiteRtLmProvider] Model file NOT FOUND at: %s"), *AbsPath);
        if (OnComplete) OnComplete(false);
        return;
    }

    UE_LOG(LogUmgMcp, Log, TEXT("[LiteRtLmProvider] Loading model from: %s"), *AbsPath);

    if (LiteRtLm->IsModelLoaded())
    {
        if (OnComplete) OnComplete(true);
        return;
    }

    if (UUmgMcpActiveMessageSubsystem* MessageSys =
            GEngine ? GEngine->GetEngineSubsystem<UUmgMcpActiveMessageSubsystem>() : nullptr)
    {
        MessageSys->UpdateActiveMessageMeta(
            TEXT("System"),
            LOCTEXT("LiteRtLmLoadingModel", "Loading local LiteRT-LM model (this may take a few seconds)..."),
            true);
    }

    // Build config with VRAM auto-detection (capped at 4GB)
    FLiteRtLmConfig Config = FLiteRtLmUnrealApi::GetAutoConfig();
    Config.ModelPath = AbsPath;
    Config.bEnableStreaming = true;
    Config.Backend = TEXT("gpu"); // Ensure GPU

    Config.MaxNumTokens = 128 * 1024; // Hardcoded 128K Context for Winyunq Demo

    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Config, OnComplete]()
    {
        bool bSuccess = FLiteRtLmUnrealApi::LoadModel(Config);
        AsyncTask(ENamedThreads::GameThread, [bSuccess, OnComplete]()
        {
            if (OnComplete)
            {
                OnComplete(bSuccess);
            }
        });
    });
}

// ============================================================
// FetchModelList
// ============================================================

void FUmgMcpLiteRtLmAiProvider::FetchModelList(
    TFunction<void(bool, const TArray<FUmgMcpModelMetadata>&, const FString&)> OnComplete)
{
    TArray<FUmgMcpModelMetadata> Meta;
    Meta.Add({ ModelFile, FPaths::GetBaseFilename(ModelFile), 2048, true });
    if (OnComplete) OnComplete(true, Meta, TEXT(""));
}

// ============================================================
// Send
// ============================================================

void FUmgMcpLiteRtLmAiProvider::Send(const TArray<TSharedPtr<FJsonObject>>& Messages)
{
    bStopRequested = false;

    // ---- Subsystems ----
    UUmgMcpActiveMessageSubsystem* MessageSys = GEngine->GetEngineSubsystem<UUmgMcpActiveMessageSubsystem>();
    UUmgMcpSessionManagerSubsystem* SessionSys = GEngine->GetEngineSubsystem<UUmgMcpSessionManagerSubsystem>();
    TSharedPtr<FUmgMcpAgent> CapturedAgent = SessionSys ? SessionSys->GetMainAgent() : nullptr;
    void* AgentPtr = CapturedAgent.IsValid() ? (void*)CapturedAgent.Get() : nullptr;

    // ---- 1. Build ToolsJson (Disabled for pure dialogue) ----
    FString ToolsJson;

    // ---- 2. Ensure model loaded ----
    TWeakPtr<FUmgMcpLiteRtLmAiProvider, ESPMode::ThreadSafe> WeakSelfProvider = this->AsShared();

    EnsureModelLoadedAsync([this, WeakSelfProvider, Messages, CapturedAgent, MessageSys, AgentPtr, ToolsJson](bool bSuccess)
    {
        TSharedPtr<FUmgMcpLiteRtLmAiProvider, ESPMode::ThreadSafe> StrongThis = WeakSelfProvider.Pin();
        if (!StrongThis.IsValid() || StrongThis->bStopRequested) return;

        if (!bSuccess)
        {
            if (MessageSys && CapturedAgent.IsValid())
            {
                MessageSys->ShowActiveErrorMessage(
                    LOCTEXT("LiteRtLmModelNotLoaded", "LiteRT-LM model not loaded. Check Settings.").ToString(),
                    CapturedAgent);
            }
            return;
        }

        if (MessageSys && CapturedAgent.IsValid())
        {
            MessageSys->UpdateActiveMessageMeta(
                CapturedAgent->Name, LOCTEXT("LiteRtLmInfer", "Inferencing..."), true);
        }

        // ---- 3. Sampling params ----
        FLiteRtLmSamplingParams Params;
        Params.MaxTokens   = 2048; // Default to 2048 for safety
        Params.Temperature = 0.7f;
        Params.TopP        = 0.9f;
        Params.TopK        = 40;

        Params.MaxTokens = 4096; // Hardcoded for 128K context window logic (streaming response cap)

        UE_LOG(LogTemp, Log, TEXT("[LiteRtLmProvider] Sending request with MaxTokens=%d"), Params.MaxTokens);

        // ---- 4. Native Streaming UI ----
        FLiteRtLmChunkCallback OnChunk = FLiteRtLmChunkCallback::CreateLambda(
            [MessageSys](const FString& Chunk)
            {
                if (MessageSys)
                {
                    AsyncTask(ENamedThreads::GameThread, [MessageSys, Chunk]()
                    {
                        MessageSys->AppendActiveMessageText(Chunk);
                    });
                }
            });

        // Done callback: finalize and deliver to Agent
        FLiteRtLmDoneCallback OnDone = FLiteRtLmDoneCallback::CreateLambda(
            [WeakSelfProvider, CapturedAgent, MessageSys]
            (const FLiteRtLmResult& Result)
            {
                TSharedPtr<FUmgMcpLiteRtLmAiProvider, ESPMode::ThreadSafe> DoneThis = WeakSelfProvider.Pin();
                if (!DoneThis.IsValid() || DoneThis->bStopRequested) return;

                FUmgMcpAiResponse AiRes;
                AiRes.bSuccess = Result.ErrorMsg.IsEmpty();
                AiRes.ResponseText = Result.FullText;
                AiRes.ToolCalls = Result.ToolCalls;

                if (!AiRes.bSuccess)
                {
                    if (MessageSys && CapturedAgent.IsValid())
                        MessageSys->ShowActiveErrorMessage(Result.ErrorMsg, CapturedAgent);
                    return;
                }

                if (CapturedAgent.IsValid())
                {
                    // Note: We've already streamed the text, but Receive will handle 
                    // archiving the message to history/session. 
                    CapturedAgent->Receive(AiRes);
                }
            });

        // ---- 5. Send via API ----
        FLiteRtLmUnrealApi::SendChatRequest(
            AgentPtr, Messages, ToolsJson, OnChunk, OnDone, Params);
    });
}

#undef LOCTEXT_NAMESPACE
