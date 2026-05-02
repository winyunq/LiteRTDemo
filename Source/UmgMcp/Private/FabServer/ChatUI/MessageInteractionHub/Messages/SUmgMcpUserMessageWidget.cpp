#include "FabServer/ChatUI/MessageInteractionHub/Messages/SUmgMcpUserMessageWidget.h"
#include "Engine/Engine.h"
// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "FabServer/ChatUI/TopBar/SUmgMcpUserAvatar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Styling/AppStyle.h"
#include "HAL/PlatformApplicationMisc.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

#include "FabServer/UmgMcpStyle.h"

void SUmgMcpUserMessageWidget::Construct(const FArguments& InArgs)
{
	MessageText = InArgs._MessageText;

	ChildSlot
	[
		SNew(SHorizontalBox)
		// 1. 消息主体区 - 靠右对齐
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.HAlign(HAlign_Right)
		.Padding(FMargin(60.0f, 0.0f, 12.0f, 0.0f))
		[
			SNew(SVerticalBox)
			// 名字
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("User")))
				.Font(FUmgMcpStyle::Get().GetWidgetStyle<FTextBlockStyle>("default").Font)
				.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.75f, 1.0f))
			]
			// 气泡
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			[
				SNew(SBorder)
				.BorderImage(FUmgMcpStyle::Get().GetBrush("UmgMcp.Bubble.User"))
				.Padding(FMargin(12.0f, 8.0f))
				[
					SNew(STextBlock) // 战术回退：使用最稳的普通文本，确保打包版 100% 可见
					.Text(FText::FromString(MessageText))
					.Font(FUmgMcpStyle::Get().GetWidgetStyle<FTextBlockStyle>("default").Font)
					.ColorAndOpacity(FLinearColor::White)
					.AutoWrapText(true)
				]
			]

			// 操作条 (复制按钮)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ContentPadding(FMargin(4.0f, 2.0f))
				.OnClicked_Lambda([this]() {
					FPlatformApplicationMisc::ClipboardCopy(*MessageText);
					return FReply::Handled();
				})
				.ToolTipText(FText::FromString(TEXT("Copy text")))
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("GenericCommands.Copy"))
					.DesiredSizeOverride(FVector2D(20, 20))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
		]

		// 2. 固定头像位
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Top)
		[
			SNew(SBox)
			.WidthOverride(128.0f)
			.HeightOverride(128.0f)
			[
				SNew(SUmgMcpUserAvatar)
			]
		]
	];
}
