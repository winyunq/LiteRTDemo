// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Styling/SlateBrush.h"
#include "FabServer/AIProviders/IUmgMcpAiProvider.h"



/**
 * FUmgMcpChatAgent: 对话 Agent 实例（实质层）。
 * 实现了 Answer -> Send -> Receive 的实质回路。
 */
class UMGMCP_API FUmgMcpAgent : public TSharedFromThis<FUmgMcpAgent>
{
public:
	FUmgMcpAgent(const FString& InRelativeJsonPath);
	virtual ~FUmgMcpAgent() = default;

	// --- 实数身份与指令 ---
	FString Name;
	TArray<FString> AllowedTools;
	TSharedPtr<FSlateBrush> AvatarBrush;
	FString SystemPrompt;

	// --- 实质回路 (The Core Circuit) ---
	/** 收到提问脉冲 (InNewSubstance 包含了这一轮互动的全部实质：文本、图片等) */
	virtual void Answer(TSharedPtr<FJsonObject> InNewSubstance);

	/** 自持的运行时上下文历史 (The Substance) */
	TArray<TSharedPtr<FJsonObject>> ContextHistory;

	/** 接收网络回执，执行归档与分拣 */
	virtual void Receive(const FUmgMcpAiResponse& InResponse);


	/** 推进实质回路：发送当前的上下文历史到网络端 */
	virtual void Send();

	/** 
	 * MCP 处理完毕的回调入口。
	 * @param Results 这一轮执行的所有 MCP 工具结果。
	 */
	virtual void OnMcpTaskFinished(const TArray<TSharedPtr<FJsonObject>>& Results);

protected:
	/** 将消息结构化为 JSON (适配 Provider，支持多模态内容块) */
	TSharedPtr<FJsonObject> CreateMessageObject(const FString& Role, const FString& Content, const TArray<FString>& InImages = {});
};
