#include "FabServer/ChatUI/MessageInteractionHub/Messages/SUmgMcpSystemNotificationWidget.h"
#include "Engine/Engine.h"
// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#if WITH_EDITOR
#include "Editor.h"
#endif
#include "FabServer/ChatSystem/UmgMcpActiveMessageSubsystem.h"
#include "FabServer/ChatUI/MessageInteractionHub/SUmgMcpMessageInteractionHub.h"

void SUmgMcpSystemNotificationWidget::Construct(const FArguments& InArgs)
{
	MessageText = InArgs._MessageText;
	bIsError = InArgs._IsError;
	OnRetryDelegate = InArgs._OnRetry;

	ChildSlot
	[
		SNew(SBox)
		.HAlign(HAlign_Center)
		.Padding(FMargin(0.0f, 4.0f))
		[
			SAssignNew(RetryButton, SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ContentPadding(FMargin(12.0f, 4.0f))
			.OnClicked(this, &SUmgMcpSystemNotificationWidget::OnRetryClicked)
			.Visibility(bIsError && OnRetryDelegate.IsBound() ? EVisibility::Visible : EVisibility::Collapsed)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Retry")))
				.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
				.ColorAndOpacity(FLinearColor(0.2f, 0.6f, 1.0f, 1.0f)) // Soft blue
			]
		]
	];
}

FReply SUmgMcpSystemNotificationWidget::OnRetryClicked()
{
	// 信号回传前，先自我销毁
	if (GEngine)
	{
		UUmgMcpActiveMessageSubsystem* ActiveSubsystem = GEngine->GetEngineSubsystem<UUmgMcpActiveMessageSubsystem>();
		if (ActiveSubsystem)
		{
			TSharedPtr<SUmgMcpMessageInteractionHub> Hub = ActiveSubsystem->GetRegisteredHub();
			if (Hub.IsValid())
			{
				// 从 Hub 的列表中物理移除当前这个失败消息
				Hub->RemoveMessageWidget(SharedThis(this));
			}
		}
	}

	// 触发重试逻辑
	OnRetryDelegate.ExecuteIfBound();

	return FReply::Handled();
}

void SUmgMcpSystemNotificationWidget::HandleActiveStateChanged(const struct FUmgMcpActiveChatStateEvent& Event)
{
}
