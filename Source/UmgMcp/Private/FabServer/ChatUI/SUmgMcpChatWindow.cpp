#include "FabServer/ChatUI/SUmgMcpChatWindow.h"
#include "Engine/Engine.h"
// Copyright (c) 2025-2026 Winyunq. All rights reserved.

#include "FabServer/ChatUI/BottomBar/SBottomBar.h"
#include "FabServer/ChatUI/MessageInteractionHub/SUmgMcpMessageInteractionHub.h"
#include "FabServer/ChatUI/TopBar/STopBar.h"
#include "FabServer/ChatUI/Welcome/SUmgMcpChatWelcome.h"
#include "FabServer/ChatUI/BottomBar/SUmgMcpChatSendButton.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#if WITH_EDITOR
#include "Editor.h"
#endif
#include "FabServer/ChatSystem/UmgMcpActiveMessageSubsystem.h"
#include "FabServer/ChatSystem/UmgMcpSessionManagerSubsystem.h"
#include "FabServer/AIProviders/UmgMcpAiSubsystem.h"
#include "FabServer/AIProviders/Local/UmgMcpLiteRtLmAiProvider.h"
#include "FabServer/AIProviders/Local/UmgMcpLiteRtLmAiProvider.h"

void SUmgMcpChatWindow::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(10.0f, 6.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(TopBarWidget, STopBar)
				.OnNewConversation(FSimpleDelegate::CreateSP(this, &SUmgMcpChatWindow::OnNewConversationClicked))
				.OnShowHistory(FSimpleDelegate::CreateSP(this, &SUmgMcpChatWindow::OnShowHistoryClicked))
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0.0f, 6.0f, 0.0f, 6.0f)
			[
				SAssignNew(MessageHubWidget, SUmgMcpMessageInteractionHub)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(ChatInputWidget, SBottomBar)
			]
		]
	];
}

SUmgMcpChatWindow::~SUmgMcpChatWindow()
{
	UE_LOG(LogTemp, Warning, TEXT("[SUmgMcpChatWindow] Destructor called. Cleaning up..."));
	if (GEngine)
	{
		if (class UUmgMcpAiSubsystem* AiSubsystem = GEngine->GetEngineSubsystem<UUmgMcpAiSubsystem>())
		{
			AiSubsystem->UnloadAllLocalProviders();
		}
	}
}

void SUmgMcpChatWindow::OnShowHistoryClicked()
{
	if (GEngine)
	{
		if (auto* Subsystem = GEngine->GetEngineSubsystem<UUmgMcpActiveMessageSubsystem>())
		{
			if (Subsystem->IsGenerating()) return;
		}
	}

	if (MessageHubWidget.IsValid())
	{
		MessageHubWidget->ClearMessages();
	}

	if (GEngine)
	{
		if (UUmgMcpSessionManagerSubsystem* SessionSubsystem = GEngine->GetEngineSubsystem<UUmgMcpSessionManagerSubsystem>())
		{
			SessionSubsystem->ClearActiveSession();
		}
	}
}

void SUmgMcpChatWindow::OnNewConversationClicked()
{
	if (GEngine)
	{
		if (auto* Subsystem = GEngine->GetEngineSubsystem<UUmgMcpActiveMessageSubsystem>())
		{
			if (Subsystem->IsGenerating()) return;
		}
	}

	if (MessageHubWidget.IsValid())
	{
		MessageHubWidget->ClearMessages();
	}

	if (GEngine)
	{
		if (UUmgMcpSessionManagerSubsystem* SessionSubsystem = GEngine->GetEngineSubsystem<UUmgMcpSessionManagerSubsystem>())
		{
			SessionSubsystem->ClearActiveSession();
		}

		if (UUmgMcpActiveMessageSubsystem* ActiveSubsystem = GEngine->GetEngineSubsystem<UUmgMcpActiveMessageSubsystem>())
		{
			ActiveSubsystem->StartNewChatMessage();
		}
	}
}

void SUmgMcpChatWindow::OnWelcomeSessionSelected(const FString& SessionId)
{
}
