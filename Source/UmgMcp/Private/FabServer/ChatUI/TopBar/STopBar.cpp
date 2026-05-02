// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "FabServer/ChatUI/TopBar/STopBar.h"

#include "FabServer/ChatUI/TopBar/SUmgMcpUserAvatar.h"
#include "FabServer/ChatUI/TopBar/SUmgMcpEditableName.h"
#include "FabServer/ChatSystem/UmgMcpActiveMessageSubsystem.h"
#if WITH_EDITOR
#include "Editor.h"
#endif
#include "LiteRtLmSubsystem.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"

#include "FabServer/UmgMcpStyle.h"

void STopBar::Construct(const FArguments& InArgs)
{
	OnNewConversationDelegate = InArgs._OnNewConversation;
	OnShowHistoryDelegate = InArgs._OnShowHistory;

	LocalUserName = TEXT("Guest");
	bIsLoggedIn = false;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(10.0f, 5.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SAssignNew(AvatarWidget, SUmgMcpUserAvatar)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(10.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SUmgMcpEditableName)
				.UserName(this, &STopBar::GetLocalUserName)
				.IsLoggedIn(this, &STopBar::GetIsLoggedIn)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SSpacer)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(10.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(this, &STopBar::GetVramStatusText)
				.Font(FUmgMcpStyle::Get().GetWidgetStyle<FTextBlockStyle>("default").Font)
				.ColorAndOpacity(FLinearColor(0.5f, 1.0f, 0.5f, 0.7f))
			]
		]
	];
}

FReply STopBar::OnNewConversationClicked()
{
	// 1. 仅保留形式通知用于 UI 页面切换
	OnNewConversationDelegate.ExecuteIfBound();
	return FReply::Handled();
}

FReply STopBar::OnShowHistoryClicked()
{
	OnShowHistoryDelegate.ExecuteIfBound();
	return FReply::Handled();
}

// ... 辅助函数省略 ...
FString STopBar::GetLocalUserName() const { return LocalUserName; }
bool STopBar::GetIsLoggedIn() const { return bIsLoggedIn; }
FText STopBar::GetHistoryToolTip() const { return FText::FromString(TEXT("History")); }
FText STopBar::GetNewConvToolTip() const { return FText::FromString(TEXT("New Conversation")); }

FText STopBar::GetVramStatusText() const
{
	int32 VramMB = ULiteRtLmSubsystem::QueryAvailableVramMB();
	return FText::Format(FText::FromString(TEXT("VRAM: {0} MB")), FText::AsNumber(VramMB));
}
