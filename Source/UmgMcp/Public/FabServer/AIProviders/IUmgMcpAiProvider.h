// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * FUmgMcpAiResponse
 * AI 返回的实质内容包。
 */
struct FUmgMcpAiResponse
{
    /** 是否成功 */
    bool bSuccess = false;



    /** 回复文本 */
    FString ResponseText;

    /** 提取出的工具调用列表 */
    TArray<TSharedPtr<FJsonObject>> ToolCalls;

    /** 错误消息 */
    FString ErrorMessage;
};

/**
 * FUmgMcpModelMetadata
 * 模型元数据，用于 UI 列表和模型选择依据。
 */
struct FUmgMcpModelMetadata
{
    FString Name;
    FString DisplayName;
    int32 InputTokenLimit = 0;
    bool bIsCliCompatible = false;
};

/**
 * IUmgMcpAiProvider
 * 远程 AI 模型的通用抽象接口。
 * 定位：纯粹的消息管道，不持有业务状态，不访问全局存储。
 */
class IUmgMcpAiProvider
{
public:
    virtual ~IUmgMcpAiProvider() {}

    /**
     * 发送对话请求 (排毒版实质接口)。
     * @param Messages 完整的对话上下文数组（由 Agent 自持并传给 Provider）。
     */
    virtual void Send(
        const TArray<TSharedPtr<FJsonObject>>& Messages
    ) = 0;

    /** 获取该提供商支持的模型列表 */
    virtual void FetchModelList(TFunction<void(bool bSuccess, const TArray<struct FUmgMcpModelMetadata>& Models, const FString& Error)> OnComplete);

    /** 判断是否为速率限制错误。 */
    static bool IsRateLimitError(const FString& ErrorMessage);

    /** 速率限制时的友好提示。 */
    static FString GetRateLimitFriendlyMessage();

    /** 检查提供商就绪状态 */
    virtual bool IsAvailable() const;

    /** 中断当前推理/请求 */
    virtual void Stop() {}

    /** 获取提供商名称 */
    virtual FString GetProviderName() const { return TEXT("Base"); }
    
    /** 获取模型 ID 等标签 */
    virtual FString GetConvergenceLabel() const { return TargetModel; }

    // --- 通用配置 (Common Configs) ---
    FString ProviderId;
    FString BaseUrl;
    FString ApiKey;
    FString TargetModel;

protected:
    /** 辅助：构建基础 OpenAI 风格 Payload (纯净版，不包含 tools，由子类按需注入) */
    virtual TSharedPtr<FJsonObject> BuildBasePayload(const TArray<TSharedPtr<FJsonObject>>& Messages);
};
