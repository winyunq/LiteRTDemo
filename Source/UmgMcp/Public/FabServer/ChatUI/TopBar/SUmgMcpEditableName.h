// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

// 头部栏用户名组件：非登录态支持“点击编辑”逻辑，登录态只读
class UMGMCP_API SUmgMcpEditableName : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUmgMcpEditableName)
		: _IsLoggedIn(false)
	{}
		SLATE_ATTRIBUTE(FString, UserName)
		SLATE_ATTRIBUTE(bool, IsLoggedIn)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FText GetNameText() const;
	
	// 显隐逻辑
	EVisibility GetReadOnlyVisibility() const;
	EVisibility GetEditableVisibility() const;
	EVisibility GetEditButtonVisibility() const;

	// 交互逻辑
	FReply OnEditClicked();
	void OnNameCommitted(const FText& NewText, ETextCommit::Type CommitType);

	TAttribute<FString> UserName;
	TAttribute<bool> IsLoggedIn;
	
	// 本地状态：是否正在编辑
	bool bIsEditing = false;

	TSharedPtr<class SEditableText> EditableText;
};
