// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "UmgMcpSessionManagerSubsystem.h"
#include "UmgMcpActiveMessageSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogFabServerFlow, Log, All);

class FUmgMcpAgent;
class SUmgMcpMessageInteractionHub;
class SUmgMcpAgentResponseGroup;
class SUmgMcpChatSendButton;
class SUmgMcpSystemNotificationWidget;
class SUmgMcpChatInput;
class SUmgMcpAttachmentList;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnUmgMcpInteractionModeChanged, const FString&);
/**
 * UUmgMcpActiveMessageSubsystem
 * 活跃对话中枢：调度实质回路，并指挥前端输出。
 */
UCLASS()
class UMGMCP_API UUmgMcpActiveMessageSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	// --- 前端形式注册 ---
	void RegisterMessageHub(TSharedPtr<SUmgMcpMessageInteractionHub> InHub);
	TSharedPtr<SUmgMcpMessageInteractionHub> GetRegisteredHub() const { return RegisteredHub.Pin(); }
	void RegisterSendButton(TSharedPtr<SUmgMcpChatSendButton> InButton);
	
	void RegisterChatInput(TSharedPtr<SUmgMcpChatInput> InInput);
	void RegisterAttachmentList(TSharedPtr<SUmgMcpAttachmentList> InList);

	// --- 状态访问 ---
	bool IsGenerating() const { return bIsGenerating; }
	FString GetInteractionMode() const { return CurrentInteractionMode; }
	void SetInteractionMode(const FString& NewMode);
	FOnUmgMcpInteractionModeChanged OnInteractionModeChanged;

	// --- 定界符 (The Spacebar) ---
	/** 启动新消息：确保当前活跃槽位是干净的。 */
	void StartNewChatMessage();

	// --- 实质调度 ---
	/** 执行发送逻辑：从注册的控件收集数据并触发请求 */
	void ExecuteSendMessage();

	void RequestQuestion(const FString& QuestionText, const TArray<FString>& InImages = {});
	void ShowActiveErrorMessage(const FString& ErrorMessage, TSharedPtr<FUmgMcpAgent> Agent);

	/** 获取当前正在生成的活跃消息气泡控件 */
	TSharedPtr<SUmgMcpAgentResponseGroup> GetActiveMessageWidget() const { return ActiveMessageWidget; }

	// --- 控制权交还 ---
	void ReturnToUser();

	void UpdateActiveMessageMeta(const FString& AgentName, const FText& Status, bool bShowSpinner);
	void AppendActiveMessageText(const FString& PartialText);
	void AddActiveMessageToolCall(const FString& ToolName, const FText& Status, bool bIsError);

	void AddCustomWidgetToActiveMessage(TSharedRef<SWidget> CustomWidget);
	void RemoveCustomWidgetFromActiveMessage(TSharedRef<SWidget> CustomWidget);

	/** 快捷方式：由输入框调用，直接向附件列表添加图片 */
	void AddAttachmentBase64(const FString& Base64);

private:
	void ClearPendingRetryNotification();

	TWeakPtr<SUmgMcpMessageInteractionHub> RegisteredHub;
	TWeakPtr<SUmgMcpChatSendButton> RegisteredSendButton;
	TWeakPtr<SUmgMcpChatInput> RegisteredChatInput;
	TWeakPtr<SUmgMcpAttachmentList> RegisteredAttachmentList;

	TSharedPtr<SUmgMcpAgentResponseGroup> ActiveMessageWidget;
	TWeakPtr<SUmgMcpSystemNotificationWidget> PendingRetryNotification;
	uint64 RequestSerial = 0;

	bool bIsGenerating = false;
	FString CurrentInteractionMode = TEXT("Chat");
};
