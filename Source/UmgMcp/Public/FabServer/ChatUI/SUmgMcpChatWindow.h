// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class STopBar;
class SUmgMcpChatWelcome;
class SUmgMcpMessageInteractionHub;
class SBottomBar;

class UMGMCP_API SUmgMcpChatWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUmgMcpChatWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SUmgMcpChatWindow();

private:
	void OnShowHistoryClicked();
	void OnNewConversationClicked();
	void OnWelcomeSessionSelected(const FString& SessionId);

	TSharedPtr<STopBar> TopBarWidget;
	TSharedPtr<SUmgMcpMessageInteractionHub> MessageHubWidget;
	TSharedPtr<SBottomBar> ChatInputWidget;
};
