// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FUmgMcpConversationHistoryNormalizer
{
public:
    static TArray<TSharedPtr<class FJsonObject>> Normalize(const TArray<TSharedPtr<class FJsonObject>>& InConversationHistory);
    
    /** 将带有属性的 HTML 标签 (如 <h4 style="...">) 清洗为 UE 认识的简单标签 (如 <h4>) */
    static FString CleanHtmlForRichText(const FString& InHtml);

private:
    static FString ExtractTextFromParts(const TArray<TSharedPtr<FJsonValue>>& Parts);
};
