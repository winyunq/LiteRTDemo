// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "FabServer/AIProviders/IUmgMcpAiProvider.h"

/**
 * FUmgMcpLiteRtLmAiProvider
 * Local AI provider backed by the LiteRT-LM Unreal plugin.
 * Thin adapter layer – delegates all inference to FLiteRtLmUnrealApi::SendChatRequest().
 */
class UMGMCP_API FUmgMcpLiteRtLmAiProvider
    : public IUmgMcpAiProvider
    , public TSharedFromThis<FUmgMcpLiteRtLmAiProvider, ESPMode::ThreadSafe>
{
public:
    explicit FUmgMcpLiteRtLmAiProvider(const FString& InModelFile);
    virtual ~FUmgMcpLiteRtLmAiProvider() override;

    // IUmgMcpAiProvider interface
    virtual void Send(const TArray<TSharedPtr<FJsonObject>>& Messages) override;
    virtual void FetchModelList(
        TFunction<void(bool bSuccess, const TArray<FUmgMcpModelMetadata>& Models, const FString& Error)> OnComplete) override;
    virtual bool IsAvailable() const override { return !ModelFile.IsEmpty(); }
    virtual FString GetProviderName() const override { return TEXT("LiteRtLmLocal"); }
    virtual void Stop() override;
    virtual FString GetConvergenceLabel() const override { return ModelFile; }

    /** Unload the LiteRT-LM engine, releasing all GPU VRAM. Idempotent. */
    virtual void UnloadModel();

private:
    /** Ensure the LiteRT-LM engine is loaded asynchronously with the configured model. */
    void EnsureModelLoadedAsync(TFunction<void(bool)> OnComplete);

    /** Model file name or absolute path (*.litertlm). */
    FString ModelFile;

    /** Set to true by Stop() so the inference callback discards the result. */
    TAtomic<bool> bStopRequested { false };
};
