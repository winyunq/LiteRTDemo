// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "FabServer/ChatUI/Welcome/SUmgMcpChatHistoryItem.h"

#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SUmgMcpChatHistoryItem::Construct(const FArguments& InArgs)
{
	SessionId = InArgs._SessionId;
	Title = InArgs._Title;
	MessageCount = InArgs._MessageCount;
	OnClickedDelegate = InArgs._OnClicked;

	ChildSlot[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(10.0f, 6.0f))
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "FlatButton")
			.OnClicked(this, &SUmgMcpChatHistoryItem::HandleClicked)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Title))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::AsNumber(MessageCount))
				]
			]
		]
	];
}

FReply SUmgMcpChatHistoryItem::HandleClicked()
{
	if (OnClickedDelegate.IsBound())
	{
		OnClickedDelegate.Execute(SessionId);
	}
	return FReply::Handled();
}
