#include "FabServer/ChatUI/MessageInteractionHub/SUmgMcpMessageInteractionHub.h"
#include "FabServer/History/UmgMcpConversationHistoryNormalizer.h"
#include "Engine/Engine.h"
// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "FabServer/ChatSystem/UmgMcpSessionManagerSubsystem.h"
#include "FabServer/ChatSystem/UmgMcpActiveMessageSubsystem.h"
#include "FabServer/ChatUI/MessageInteractionHub/Messages/SUmgMcpUserMessageWidget.h"
#include "FabServer/ChatUI/MessageInteractionHub/Messages/SUmgMcpAgentResponseGroup.h"
#include "FabServer/ChatUI/MessageInteractionHub/Messages/SUmgMcpSystemNotificationWidget.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "FabServer/ChatUI/Welcome/SUmgMcpChatWelcome.h"

void SUmgMcpMessageInteractionHub::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SAssignNew(WelcomeWidget, SUmgMcpChatWelcome)
			.OnSessionSelected_Lambda([this](const FString& SessionId) {
				// 欢迎页面选择了会话
			})
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.DarkGroupBorder"))
			.Padding(FMargin(8.0f))
			[
				SAssignNew(ScrollBoxWidget, SScrollBox)
				.ScrollBarVisibility(EVisibility::Collapsed)
				+ SScrollBox::Slot()
				[
					SAssignNew(MessageList, SVerticalBox)
				]
			]
		]
	];

	if (GEngine)
	{
		// 1. 注册实时消息流控 (ActiveSubsystem)
		UUmgMcpActiveMessageSubsystem* ActiveSubsystem = GEngine->GetEngineSubsystem<UUmgMcpActiveMessageSubsystem>();
		if (ActiveSubsystem)
		{
			ActiveSubsystem->RegisterMessageHub(SharedThis(this));
		}

		// 2. 绑定持久化与同步信号 (SessionManagerSubsystem)
		UUmgMcpSessionManagerSubsystem* SessionSubsystem = GEngine->GetEngineSubsystem<UUmgMcpSessionManagerSubsystem>();
		if (SessionSubsystem)
		{
			// 使用 AddSP 确保在 Shipping 打包版中信号链路 100% 稳健
			SessionSubsystem->OnMessageAppended.AddSP(this, &SUmgMcpMessageInteractionHub::OnMessageAppended);
			SessionSubsystem->OnSessionResumed.AddSP(this, &SUmgMcpMessageInteractionHub::OnSessionResumed);
			SessionSubsystem->OnSessionDeleted.AddSP(this, &SUmgMcpMessageInteractionHub::OnSessionDeleted);
		}
	}

	RefreshVisibility();
}

void SUmgMcpMessageInteractionHub::AddMessageWidget(TSharedRef<SWidget> InWidget)
{
	if (!MessageList.IsValid()) return;
	
	MessageList->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 10.0f)
	[
		InWidget
	];

	if (ScrollBoxWidget.IsValid())
	{
		ScrollBoxWidget->ScrollToEnd();
	}
	RefreshVisibility();
}

void SUmgMcpMessageInteractionHub::MoveWidgetToBottom(TSharedRef<SWidget> InWidget)
{
	if (!MessageList.IsValid()) return;

	for (int32 i = 0; i < MessageList->NumSlots(); ++i)
	{
		if (MessageList->GetSlot(i).GetWidget() == InWidget)
		{
			MessageList->RemoveSlot(InWidget);
			break;
		}
	}

	AddMessageWidget(InWidget);
}

void SUmgMcpMessageInteractionHub::RemoveMessageWidget(TSharedRef<SWidget> InWidget)
{
	if (MessageList.IsValid())
	{
		MessageList->RemoveSlot(InWidget);
		RefreshVisibility();
	}
}

void SUmgMcpMessageInteractionHub::ClearMessages()
{
	if (MessageList.IsValid())
	{
		MessageList->ClearChildren();
		RefreshVisibility();
	}
}

void SUmgMcpMessageInteractionHub::RefreshVisibility()
{
	bool bHasVisibleMessages = false;
	if (MessageList.IsValid())
	{
		FChildren* Children = MessageList->GetChildren();
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			if (Children->GetChildAt(i)->GetVisibility().IsVisible())
			{
				bHasVisibleMessages = true;
				break;
			}
		}
	}

	if (WelcomeWidget.IsValid())
	{
		WelcomeWidget->SetVisibility(bHasVisibleMessages ? EVisibility::Collapsed : EVisibility::Visible);
	}
	
	if (ScrollBoxWidget.IsValid())
	{
		ScrollBoxWidget->SetVisibility(bHasVisibleMessages ? EVisibility::Visible : EVisibility::Collapsed);
	}
}

void SUmgMcpMessageInteractionHub::OnMessageAppended(const FUmgMcpSessionMessage& Message)
{
	AddMessageWidget(CreateWidgetFromMessage(Message));
}

void SUmgMcpMessageInteractionHub::OnSessionResumed(const FUmgMcpSessionData& SessionData)
{
	ClearMessages();
	for (const FUmgMcpSessionMessage& Msg : SessionData.Messages)
	{
		AddMessageWidget(CreateWidgetFromMessage(Msg));
	}
}

void SUmgMcpMessageInteractionHub::OnSessionDeleted(const FString& SessionId)
{
	ClearMessages();
}

TSharedRef<SWidget> SUmgMcpMessageInteractionHub::CreateWidgetFromMessage(const FUmgMcpSessionMessage& Message)
{
	FString CleanedContent = FUmgMcpConversationHistoryNormalizer::CleanHtmlForRichText(Message.Content);
	if (CleanedContent.IsEmpty()) CleanedContent = TEXT("...");

	switch (Message.Role)
	{
	case EUmgMcpSessionRole::User:
		return SNew(SUmgMcpUserMessageWidget).MessageText(CleanedContent);
	case EUmgMcpSessionRole::Agent:
		return SNew(SUmgMcpAgentResponseGroup).AgentName(Message.AgentName).MessageText(CleanedContent);
	case EUmgMcpSessionRole::System:
		return SNew(SUmgMcpSystemNotificationWidget).MessageText(CleanedContent);
	default:
		return SNew(SUmgMcpUserMessageWidget).MessageText(CleanedContent);
	}
}
