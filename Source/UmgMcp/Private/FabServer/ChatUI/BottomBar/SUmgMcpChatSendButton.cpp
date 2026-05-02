#include "FabServer/ChatUI/BottomBar/SUmgMcpChatSendButton.h"
#include "Engine/Engine.h"
// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "FabServer/ChatUI/BottomBar/SUmgMcpChatInput.h"
#include "FabServer/ChatUI/BottomBar/SUmgMcpAttachmentList.h"
#include "FabServer/ChatSystem/UmgMcpActiveMessageSubsystem.h"
#if WITH_EDITOR
#include "Editor.h"
#endif
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SThrobber.h"
#include "Styling/AppStyle.h"
#include "Misc/DateTime.h"

void SUmgMcpChatSendButton::Construct(const FArguments& InArgs)
{

	ChildSlot
	[
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
		.OnClicked(this, &SUmgMcpChatSendButton::HandleOnClicked)
		.ContentPadding(FMargin(16.0f, 4.0f))
		.ButtonColorAndOpacity(this, &SUmgMcpChatSendButton::GetButtonBackgroundColor)
		[
			SNew(SBox)
			.MinDesiredWidth(60.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &SUmgMcpChatSendButton::GetButtonText)
					.Font(FAppStyle::Get().GetFontStyle("NormalFont"))
					.ColorAndOpacity(this, &SUmgMcpChatSendButton::GetButtonColor)
				]

				// Generating State (Throbber visible ONLY when running)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SThrobber)
					.PieceImage(FAppStyle::GetBrush("Throbber.CircleChunk"))
					.NumPieces(3)
					.Animate(SThrobber::Opacity)
					.Visibility_Lambda([this]() {
						return bIsRunning ? EVisibility::Visible : EVisibility::Collapsed;
					})
				]
			]
		]
	];

	if (GEngine)
	{
		if (auto* Subsystem = GEngine->GetEngineSubsystem<UUmgMcpActiveMessageSubsystem>())
		{
			Subsystem->RegisterSendButton(SharedThis(this));
		}
	}
}

void SUmgMcpChatSendButton::SetIsRunning(bool bRunning)
{
	bIsRunning = bRunning;
	bInterruptConfirmArmed = false;
}

bool SUmgMcpChatSendButton::IsRunning() const
{
	if (GEngine)
	{
		if (auto* Subsystem = GEngine->GetEngineSubsystem<UUmgMcpActiveMessageSubsystem>())
		{
			return Subsystem->IsGenerating();
		}
	}
	return bIsRunning;
}

FReply SUmgMcpChatSendButton::HandleOnClicked()
{
	if (GEngine)
	{
		if (auto* Subsystem = GEngine->GetEngineSubsystem<UUmgMcpActiveMessageSubsystem>())
		{
			if (Subsystem->IsGenerating())
			{
				FDateTime Now = FDateTime::UtcNow();
				if (!bInterruptConfirmArmed || Now > InterruptConfirmExpireAt)
				{
					bInterruptConfirmArmed = true;
					InterruptConfirmExpireAt = Now + FTimespan::FromSeconds(3.0);
					return FReply::Handled();
				}

				// 中断逻辑：通过子系统停止
				Subsystem->ReturnToUser();
				return FReply::Handled();
			}

			bInterruptConfirmArmed = false;
			Subsystem->ExecuteSendMessage();
		}
	}

	return FReply::Handled();
}

FText SUmgMcpChatSendButton::GetButtonText() const
{
	bool bRunning = false;
	if (GEngine)
	{
		if (auto* Subsystem = GEngine->GetEngineSubsystem<UUmgMcpActiveMessageSubsystem>())
		{
			bRunning = Subsystem->IsGenerating();
		}
	}

	if (bRunning)
	{
		if (bInterruptConfirmArmed && FDateTime::UtcNow() <= InterruptConfirmExpireAt)
		{
			return FText::FromString(TEXT("3s内再次点击中断"));
		}
		return FText::FromString(TEXT("Interrupt"));
	}
	return FText::FromString(TEXT("Send"));
}

FSlateColor SUmgMcpChatSendButton::GetButtonBackgroundColor() const
{
	bool bRunning = false;
	if (GEngine)
	{
		if (auto* Subsystem = GEngine->GetEngineSubsystem<UUmgMcpActiveMessageSubsystem>())
		{
			bRunning = Subsystem->IsGenerating();
		}
	}

	if (bRunning)
	{
		if (bInterruptConfirmArmed && FDateTime::UtcNow() <= InterruptConfirmExpireAt)
		{
			return FLinearColor(1.0f, 0.0f, 0.0f); // Bright red
		}
		return FLinearColor(0.8f, 0.2f, 0.2f); // Dull red
	}
	return FLinearColor::White;
}

FSlateColor SUmgMcpChatSendButton::GetButtonColor() const
{
	return FSlateColor::UseForeground();
}
