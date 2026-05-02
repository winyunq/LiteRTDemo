// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

/**
 * STopBar：顶部栏核心容器。
 * 负责头像、用户名以及功能按钮（历史、新建）的原子拼装。
 */
class UMGMCP_API STopBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STopBar) {}
		SLATE_ARGUMENT(FSimpleDelegate, OnNewConversation)
		SLATE_ARGUMENT(FSimpleDelegate, OnShowHistory)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	// 修复：补全缺失的属性绑定函数声明
	FString GetLocalUserName() const;
	bool GetIsLoggedIn() const;
	FText GetHistoryToolTip() const;
	FText GetNewConvToolTip() const;
	FText GetVramStatusText() const;

	// 功能转发
	FReply OnNewConversationClicked();
	FReply OnShowHistoryClicked();

	// 事件代理
	FSimpleDelegate OnNewConversationDelegate;
	FSimpleDelegate OnShowHistoryDelegate;

	// 状态变量
	// 修复：补全缺失的变量声明
	FString LocalUserName;
	bool bIsLoggedIn;

	// 原子组件
	TSharedPtr<class SUmgMcpUserAvatar> AvatarWidget;
};
