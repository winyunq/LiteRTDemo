// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

struct FUmgMcpSessionData;
struct FUmgMcpSessionMessage;

/**
 * SUmgMcpMessageInteractionHub
 */
class UMGMCP_API SUmgMcpMessageInteractionHub : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUmgMcpMessageInteractionHub) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** 内部刷新逻辑：根据是否有可见消息自动切换 Welcome/Messages 视图 */
	void RefreshVisibility();

	/** 向列表追加一个新的消息组件 */
	void AddMessageWidget(TSharedRef<SWidget> InWidget);

	/** 将已存在的槽位移到底部 */
	void MoveWidgetToBottom(TSharedRef<SWidget> InWidget);

	/** 从列表中移除指定的 Widget */
	void RemoveMessageWidget(TSharedRef<SWidget> InWidget);

	/** 清空当前列表 */
	void ClearMessages();

private:
	// 同步历史会话
	void OnSessionResumed(const struct FUmgMcpSessionData& SessionData);
	void OnMessageAppended(const struct FUmgMcpSessionMessage& Message);
	void OnSessionDeleted(const FString& SessionId);

	// 辅助方法
	TSharedRef<SWidget> CreateWidgetFromMessage(const struct FUmgMcpSessionMessage& Message);

	TSharedPtr<class SScrollBox> ScrollBoxWidget;
	TSharedPtr<class SVerticalBox> MessageList;
	TSharedPtr<class SUmgMcpChatWelcome> WelcomeWidget;
};
