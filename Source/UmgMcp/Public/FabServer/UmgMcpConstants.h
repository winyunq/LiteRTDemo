// Copyright (c) 2025-2026 Winyunq. All rights reserved.

#pragma once

#include "CoreMinimal.h"

namespace UmgMcpConstants
{
    /** [官方 CLI 认证参数]
     *  这些参数摘录自 Gemini CLI 的公开仿真逻辑，用于 native 模式下的 Token 刷新。
     */
    namespace GeminiCli
    {
        const FString DefaultClientId = TEXT("YOUR_CLIENT_ID_HERE");
        const FString DefaultClientSecret = TEXT("YOUR_CLIENT_SECRET_HERE");
        const FString InternalApiBaseUrl = TEXT("https://cloudcode-pa.googleapis.com/v1internal");
        
        /** 硬编码的探测模型名，用于在 UI 中定位 CLI 仿真分路 */
        const FString ProbingModelName = TEXT("gemini-cli-native");
    }

    /** [Google API 标准端点] */
    namespace GoogleApi
    {
        const FString OAuthTokenUrl = TEXT("https://oauth2.googleapis.com/token");
        const FString GenerativeLanguageBaseUrl = TEXT("https://generativelanguage.googleapis.com/v1beta/");
    }

    /** [UI 物理标签] 
     *  用于在探测链收敛时进行逻辑对齐。
     */
    const FText Label_ApiKey = NSLOCTEXT("UmgMcp", "Label_ApiKey", "[API Key]");
    const FText Label_GeminiCLI = NSLOCTEXT("UmgMcp", "Label_GeminiCLI", "[Gemini CLI]");
}
