#include "FabServer/ChatUI/Welcome/SUmgMcpChatWelcome.h"
#include "Engine/Engine.h"
// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "FabServer/ChatSystem/UmgMcpSessionManagerSubsystem.h"
#if WITH_EDITOR
#include "Editor.h"
#endif

#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	FText GetLocText(const FString& English, const FString& Chinese)
	{
		const FString Culture = FInternationalization::Get().GetCurrentCulture()->GetName();
		return FText::FromString(Culture.StartsWith(TEXT("zh")) ? Chinese : English);
	}

	FString FormatDateTime(const FDateTime& DateTime)
	{
		return DateTime.ToString(TEXT("%Y-%m-%d"));
	}
}

void SUmgMcpChatWelcome::Construct(const FArguments& InArgs)
{
	OnSessionSelected = InArgs._OnSessionSelected;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(40.0f, 20.0f))
		[
			SNew(SVerticalBox)

			// 1. 标题区域
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.0f, 40.0f, 0.0f, 5.0f))
			[
				SNew(STextBlock)
				.Text(GetLocText(TEXT("LiteRT-LM AI Assistant"), TEXT("LiteRT-LM 智能助手")))
				.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
				.ColorAndOpacity(FLinearColor(1.0f, 0.8f, 0.2f))
			]

			// 2. GitHub 链接
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.0f, 0.0f, 0.0f, 40.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("("))).Font(FAppStyle::Get().GetFontStyle("SmallFont")).ColorAndOpacity(FLinearColor(0.3f, 0.3f, 0.3f))
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "NoBorder")
					.Cursor(EMouseCursor::Hand)
					.OnClicked_Lambda([]() {
						FPlatformProcess::LaunchURL(TEXT("https://github.com/winyunq/UnrealMotionGraphicsMCP"), nullptr, nullptr);
						return FReply::Handled();
					})
					[
						SNew(STextBlock)
						.Text(GetLocText(TEXT("Core service is available at github.com/winyunq/UnrealMotionGraphicsMCP"), TEXT("开源 MCP 核心服务可于 github.com/winyunq 获取")))
						.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
						.ColorAndOpacity(FLinearColor(0.3f, 0.5f, 0.9f))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(STextBlock).Text(FText::FromString(TEXT(")"))).Font(FAppStyle::Get().GetFontStyle("SmallFont")).ColorAndOpacity(FLinearColor(0.3f, 0.3f, 0.3f))
				]
			]

			// 引导提示
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.0f, 0.0f, 0.0f, 40.0f))
			[
				SNew(STextBlock)
				.Text(GetLocText(
					TEXT("There are 5 cute agents here. Pick one and have a chat!"),
					TEXT("这里有 5 个可爱的AI宝宝，选择一个，和她们聊聊吧！")))
				.Font(FAppStyle::Get().GetFontStyle("NormalFont"))
				.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f))
				.Justification(ETextJustify::Center)
			]

			// 操作区
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(0.4f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("✨")))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 60))
						.ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.1f))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.6f)
				.Padding(FMargin(20.0f, 0.0f, 0.0f, 0.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))
					[
						SNew(STextBlock)
						.Text(GetLocText(TEXT("History"), TEXT("历史记录")))
						.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SNew(SScrollBox)
							.ScrollBarThickness(FVector2D(4.0f, 4.0f))
							+ SScrollBox::Slot()
							[
								SAssignNew(HistoryListBox, SVerticalBox)
							]
						]
						+ SOverlay::Slot()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(GetLocText(TEXT("No history"), TEXT("你还没有对话过")))
							.Font(FAppStyle::Get().GetFontStyle("NormalFont"))
							.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f))
							.Visibility(this, &SUmgMcpChatWelcome::GetEmptyListVisibility)
						]
					]
				]
			]
		]
	];

	RefreshHistoryList();
}

void SUmgMcpChatWelcome::RefreshHistoryList()
{
	if (!HistoryListBox.IsValid()) return;
	HistoryListBox->ClearChildren();

	TArray<FUmgMcpSessionIndex> Sessions;
	if (GEngine)
	{
		if (UUmgMcpSessionManagerSubsystem* SessionSubsystem = GEngine->GetEngineSubsystem<UUmgMcpSessionManagerSubsystem>())
		{
			Sessions = SessionSubsystem->GetRecentSessions(10);
		}
	}

	for (const FUmgMcpSessionIndex& Session : Sessions)
	{
		HistoryListBox->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.0f, 2.0f))
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(2.0f))
			[
				// 修复：使用 Lambda 捕获 SessionId 并返回 FReply
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "FlatButton")
				.OnClicked_Lambda([this, SessionID = Session.SessionId]() {
					return OnSessionClicked(SessionID);
				})
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(8, 4)
					.VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
							.Text(FText::FromString(Session.Title))
							.Font(FAppStyle::Get().GetFontStyle("NormalFont"))
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
							.Text(FText::FromString(FormatDateTime(Session.LastModified)))
							.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
							.ColorAndOpacity(FLinearColor::Gray)
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(8, 0)
					[
						SNew(STextBlock)
						.Text(FText::AsNumber(Session.MessageCount))
						.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
						.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0, 0, 4, 0)
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
						.OnClicked_Lambda([this, SessionID = Session.SessionId]() {
							if (GEngine)
							{
								if (UUmgMcpSessionManagerSubsystem* SessionSubsystem = GEngine->GetEngineSubsystem<UUmgMcpSessionManagerSubsystem>())
								{
									SessionSubsystem->DeleteSession(SessionID);
								}
							}
							return FReply::Handled();
						})
						.ToolTipText(FText::FromString(TEXT("Delete this conversation")))
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("x")))
							.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
							.ColorAndOpacity(FLinearColor::Gray)
						]
					]
				]
			]
		];
	}
}

FReply SUmgMcpChatWelcome::OnSessionClicked(FString SessionId)
{
	// 1. 端到端直连执行实质：恢复会话
	if (GEngine)
	{
		if (UUmgMcpSessionManagerSubsystem* SessionSubsystem = GEngine->GetEngineSubsystem<UUmgMcpSessionManagerSubsystem>())
		{
			SessionSubsystem->ResumeSession(SessionId);
		}
	}

	// 2. 通知形式层切换页面
	if (OnSessionSelected.IsBound())
	{
		OnSessionSelected.Execute(SessionId);
	}
	return FReply::Handled();
}

EVisibility SUmgMcpChatWelcome::GetEmptyListVisibility() const
{
	return (HistoryListBox.IsValid() && HistoryListBox->GetChildren()->Num() > 0) ? EVisibility::Collapsed : EVisibility::Visible;
}
