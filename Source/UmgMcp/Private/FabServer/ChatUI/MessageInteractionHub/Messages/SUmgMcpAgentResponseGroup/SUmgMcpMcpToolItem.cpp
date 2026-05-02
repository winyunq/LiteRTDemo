// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "FabServer/ChatUI/MessageInteractionHub/Messages/SUmgMcpAgentResponseGroup/SUmgMcpMcpToolItem.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/AppStyle.h"

void SUmgMcpMcpToolItem::Construct(const FArguments& InArgs)
{
	ToolName = InArgs._ToolName;
	ArgumentsJson = InArgs._ArgumentsJson;

	ChildSlot
	[
		SNew(SVerticalBox)
		// 头部：工具名与状态
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(ToolName))
				.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
				.ColorAndOpacity(FLinearColor::White)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(8.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(StatusTextBlock, STextBlock)
				.Text(FText::FromString(TEXT("Pending approval...")))
				.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
				.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f))
			]
		]
		// 详情区 (可扩展展示参数)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(16.0f, 2.0f, 4.0f, 4.0f)
		[
			SAssignNew(DetailsBox, SVerticalBox)
			.Visibility(EVisibility::Collapsed) // 默认收起
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryMiddle"))
				.Padding(FMargin(8.0f))
				[
					SNew(STextBlock)
					.Text(FText::FromString(ArgumentsJson))
					.AutoWrapText(true)
					.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
					.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
				]
			]
		]
	];
}

void SUmgMcpMcpToolItem::SetStatus(EToolStatus InStatus, const FString& ErrorMessage)
{
	CurrentStatus = InStatus;
	FString StatusText;
	FLinearColor StatusColor = FLinearColor::White;

	switch (InStatus)
	{
		case EToolStatus::Pending: StatusText = TEXT("Pending approval..."); StatusColor = FLinearColor(0.5f, 0.5f, 0.5f); break;
		case EToolStatus::Running: StatusText = TEXT("Executing..."); StatusColor = FLinearColor::Yellow; break;
		case EToolStatus::Success: StatusText = TEXT("Success"); StatusColor = FLinearColor::Green; break;
		case EToolStatus::Failed: StatusText = FString::Printf(TEXT("Failed: %s"), *ErrorMessage); StatusColor = FLinearColor::Red; break;
		case EToolStatus::Rejected: StatusText = TEXT("Rejected"); StatusColor = FLinearColor::Gray; break;
	}

	if (StatusTextBlock.IsValid())
	{
		StatusTextBlock->SetText(FText::FromString(StatusText));
		StatusTextBlock->SetColorAndOpacity(StatusColor);
	}
}
