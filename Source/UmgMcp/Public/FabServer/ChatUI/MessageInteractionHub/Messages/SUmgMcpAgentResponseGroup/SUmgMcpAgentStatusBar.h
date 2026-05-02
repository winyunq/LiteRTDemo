// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"

/**
 * SUmgMcpAgentStatusBar
 * Agent状态区：包含转圈和状态文本。
 */
class UMGMCP_API SUmgMcpAgentStatusBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUmgMcpAgentStatusBar) {}
		SLATE_ARGUMENT(FString, StatusText)
		SLATE_ARGUMENT(bool, bShowSpinner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetStatus(const FString& InStatusText, bool bInShowSpinner, const FLinearColor& StatusColor = FLinearColor::White);

private:
	FReply OnCopyButtonClicked();

	TSharedPtr<class SThrobber> SpinnerWidget;
	TSharedPtr<class STextBlock> StatusTextWidget;
};
