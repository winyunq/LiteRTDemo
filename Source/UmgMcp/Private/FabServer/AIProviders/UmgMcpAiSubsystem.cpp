#include "FabServer/AIProviders/UmgMcpAiSubsystem.h"
#include "UmgMcp.h"
#include "FabServer/AIProviders/Local/UmgMcpLiteRtLmAiProvider.h"

#include "FabServer/UmgMcpConstants.h"
#include "FabServer/ChatSystem/UmgMcpActiveMessageSubsystem.h"
#include "Logging/LogMacros.h"
#include "Misc/ScopeLock.h"

void UUmgMcpAiSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Hardcode LiteRT-LM provider with the specific model
    const FString HardcodedModel = TEXT("gemma-4-E2B-it.litertlm");
    CurrentProvider_LiteRtLm = MakeShared<FUmgMcpLiteRtLmAiProvider>(HardcodedModel);
    CurrentProvider_LiteRtLm->ProviderId = TEXT("litertlm:hardcoded");

    ProbeChain.Empty();
    ProbeChain.Add(CurrentProvider_LiteRtLm);

    UE_LOG(LogUmgMcp, Log, TEXT("UmgMcpAiSubsystem Initialized with hardcoded LiteRT-LM."));
}

void UUmgMcpAiSubsystem::RebuildCandidates()
{
    // Simplified: always use the hardcoded LiteRT-LM provider
    if (ProbeChain.Num() == 0 || !ProbeChain[0].IsValid())
    {
        ProbeChain.Empty();
        ProbeChain.Add(CurrentProvider_LiteRtLm);
    }
}

void UUmgMcpAiSubsystem::Deinitialize()
{
    CurrentProvider_LiteRtLm.Reset();
    Super::Deinitialize();
}

void UUmgMcpAiSubsystem::RequestCancel()
{
    TSharedPtr<IUmgMcpAiProvider> CurrentProvider = GetProviderForCurrentMode();
    if (CurrentProvider.IsValid())
    {
        CurrentProvider->Stop();
    }

    {
        FScopeLock ScopeLock(&RequestStateCs);
        bCancelRequested = true;
        bIsBusy = false;
    }
    OnAiRequestFinished.Broadcast();
}

bool UUmgMcpAiSubsystem::TryCancelIfBusy()
{
    bool bDidCancel = false;
    {
        FScopeLock ScopeLock(&RequestStateCs);
        if (bIsBusy)
        {
            bCancelRequested = true;
            bIsBusy = false;
            bDidCancel = true;
        }
    }
    if (bDidCancel)
    {
        OnAiRequestFinished.Broadcast();
    }
    return bDidCancel;
}

bool UUmgMcpAiSubsystem::IsBusy() const
{
    FScopeLock ScopeLock(&RequestStateCs);
    return bIsBusy;
}

bool UUmgMcpAiSubsystem::IsCancelRequested() const
{
    FScopeLock ScopeLock(&RequestStateCs);
    return bCancelRequested;
}

TSharedPtr<IUmgMcpAiProvider> UUmgMcpAiSubsystem::GetProviderForCurrentMode()
{
    return CurrentProvider_LiteRtLm;
}

void UUmgMcpAiSubsystem::SetActiveProvider(const FString& ProviderName)
{
}

FString UUmgMcpAiSubsystem::GetProviderName() const
{
    return TEXT("LiteRT-LM (Hardcoded)");
}

void UUmgMcpAiSubsystem::UnloadAllLocalProviders()
{
    if (CurrentProvider_LiteRtLm.IsValid())
    {
        StaticCastSharedPtr<FUmgMcpLiteRtLmAiProvider>(CurrentProvider_LiteRtLm)->UnloadModel();
    }
}

void UUmgMcpAiSubsystem::FetchModelList(TFunction<void(bool bSuccess, const TArray<FUmgMcpModelMetadata>& Models, const FString& Error)> OnComplete)
{
    if (CurrentProvider_LiteRtLm.IsValid())
    {
        CurrentProvider_LiteRtLm->FetchModelList([this, OnComplete](bool bSuccess, const TArray<FUmgMcpModelMetadata>& Models, const FString& Error) {
            if (bSuccess) CachedModels = Models;
            if (OnComplete) OnComplete(bSuccess, Models, Error);
        });
    }
}

void UUmgMcpAiSubsystem::Send(const TArray<TSharedPtr<FJsonObject>>& Messages)
{
    {
        FScopeLock ScopeLock(&RequestStateCs);
        bCancelRequested = false;
        bIsBusy = true;
    }

    TSharedPtr<IUmgMcpAiProvider> CurrentProvider = GetProviderForCurrentMode();
    if (CurrentProvider.IsValid())
    {
        OnAiRequestStarted.Broadcast();
        CurrentProvider->Send(Messages);
    }
}
