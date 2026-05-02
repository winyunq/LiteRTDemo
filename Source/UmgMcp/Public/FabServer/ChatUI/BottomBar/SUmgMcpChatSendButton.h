// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SUmgMcpChatInput;
class SUmgMcpAttachmentList;

/**
 * SUmgMcpChatSendButton
 * 发送按钮：还原旧版具有 SThrobber 动画和动态颜色反馈的 UI 形式。
 */
class UMGMCP_API SUmgMcpChatSendButton : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUmgMcpChatSendButton) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** 设置按钮状态 */
	void SetIsRunning(bool bRunning);
	bool IsRunning() const;

	FReply HandleOnClicked();

private:
	// UI 数据绑定函数
	FText GetButtonText() const;
	FSlateColor GetButtonBackgroundColor() const;
	FSlateColor GetButtonColor() const;

	// 状态管理
	bool bIsRunning = false;

	// 中断拦截逻辑
	bool bInterruptConfirmArmed = false;
	FDateTime InterruptConfirmExpireAt;
};
