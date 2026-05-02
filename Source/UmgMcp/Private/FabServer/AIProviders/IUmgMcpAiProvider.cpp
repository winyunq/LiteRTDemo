// Copyright (c) 2025-2026 Winyunq. All rights reserved.

#include "FabServer/AIProviders/IUmgMcpAiProvider.h"
#include "Dom/JsonObject.h"

void IUmgMcpAiProvider::FetchModelList(TFunction<void(bool bSuccess, const TArray<FUmgMcpModelMetadata>& Models, const FString& Error)> OnComplete)
{
    if (OnComplete) OnComplete(false, {}, TEXT("Not implemented"));
}

bool IUmgMcpAiProvider::IsRateLimitError(const FString& ErrorMessage)
{
    return ErrorMessage.Contains(TEXT("429")) || ErrorMessage.Contains(TEXT("rate_limit"));
}

FString IUmgMcpAiProvider::GetRateLimitFriendlyMessage()
{
    return TEXT("Rate limit reached. Please try again later.");
}

bool IUmgMcpAiProvider::IsAvailable() const
{
    return !ApiKey.IsEmpty();
}

TSharedPtr<FJsonObject> IUmgMcpAiProvider::BuildBasePayload(const TArray<TSharedPtr<FJsonObject>>& Messages)
{
    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    
    TArray<TSharedPtr<FJsonValue>> JsonMessages;
    for (const auto& Msg : Messages)
    {
        if (Msg.IsValid())
        {
            JsonMessages.Add(MakeShared<FJsonValueObject>(Msg));
        }
    }
    
    Root->SetArrayField(TEXT("messages"), JsonMessages);
    Root->SetStringField(TEXT("model"), TargetModel);
    
    return Root;
}
