// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SUmgMcpChatInput;
class SUmgMcpAbilitiesSelector;
class SUmgMcpInteractionModeSelector;
class SUmgMcpToolModeSelector;
class SUmgMcpChatSendButton;
class SUmgMcpQuotaBar;
class SUmgMcpAttachmentList;

/**
 * SBottomBar
 */
class UMGMCP_API SBottomBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBottomBar) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// 原子微控件 (Public 为指挥官提供直达访问)
	TSharedPtr<SUmgMcpAbilitiesSelector> AbilitiesSelector;
	TSharedPtr<SUmgMcpChatInput> ChatInput;
	TSharedPtr<SUmgMcpInteractionModeSelector> InteractionModeSelector;
	TSharedPtr<SUmgMcpToolModeSelector> ToolModeSelector;
	TSharedPtr<SUmgMcpChatSendButton> SendButton;
	TSharedPtr<class SUmgMcpAttachmentList> AttachmentList;
	TSharedPtr<SUmgMcpQuotaBar> QuotaBar;

private:
	// 事件回调
	void OnInteractionModeChanged(const FString& NewMode);
	void OnToolModeChanged(const FString& NewTool);
	FReply OnSendClicked();
	void OnChatInputSendRequested();
	void OnChatInputPasteImage(const TArray<uint8>& ImageData, int32 Width, int32 Height);
	FReply OnAddAttachmentClicked();
};
