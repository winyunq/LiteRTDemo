#include "FabServer/ChatUI/BottomBar/SUmgMcpChatInput.h"
#include "Engine/Engine.h"
// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/SBoxPanel.h"

#include "FabServer/FabWindowsClipboard.h"
#include "ImageUtils.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "FabServer/ChatSystem/UmgMcpActiveMessageSubsystem.h"
#if WITH_EDITOR
#include "Editor.h"
#endif

namespace
{
	FText GetLocText(const FString& English, const FString& Chinese)
	{
		const FString Culture = FInternationalization::Get().GetCurrentCulture()->GetName();
		return FText::FromString(Culture.StartsWith(TEXT("zh")) ? Chinese : English);
	}
}

void SUmgMcpChatInput::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SAssignNew(InputTextBox, SMultiLineEditableTextBox)
		.AutoWrapText(true)
		.HintText(GetLocText(TEXT("Type a message... (Ctrl+Enter to Send)"), TEXT("输入消息... (Ctrl+Enter 发送)")))
		.OnKeyDownHandler(this, &SUmgMcpChatInput::OnInputKeyDown)
	];

	if (GEngine)
	{
		if (auto* Subsystem = GEngine->GetEngineSubsystem<UUmgMcpActiveMessageSubsystem>())
		{
			Subsystem->RegisterChatInput(SharedThis(this));
		}
	}
}

FText SUmgMcpChatInput::GetText() const
{
	return InputTextBox.IsValid() ? InputTextBox->GetText() : FText::GetEmpty();
}

void SUmgMcpChatInput::SetText(const FText& InText)
{
	if (InputTextBox.IsValid())
	{
		InputTextBox->SetText(InText);
	}
}

void SUmgMcpChatInput::ClearText()
{
	SetText(FText::GetEmpty());
}

FReply SUmgMcpChatInput::OnInputKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	// 发送快捷键：Enter（当未按Shift时）或Ctrl+Enter
	bool bIsEnter = InKeyEvent.GetKey() == EKeys::Enter;
	bool bHasCtrl = InKeyEvent.IsControlDown();
	bool bHasShift = InKeyEvent.IsShiftDown();

	if (bIsEnter && (!bHasShift || bHasCtrl))
	{
		if (GEngine)
		{
			if (auto* Subsystem = GEngine->GetEngineSubsystem<UUmgMcpActiveMessageSubsystem>())
			{
				Subsystem->ExecuteSendMessage();
			}
		}
		return FReply::Handled();
	}

#if PLATFORM_WINDOWS
	// 粘贴图片快捷键：Ctrl+V
	if (bHasCtrl && InKeyEvent.GetKey() == EKeys::V)
	{
		int32 Width, Height;
		TArray<uint8> PendingImageData;
		if (FFabWindowsClipboard::GetBitmapFromClipboard(PendingImageData, Width, Height))
		{
			if (GEngine)
			{
				if (auto* Subsystem = GEngine->GetEngineSubsystem<UUmgMcpActiveMessageSubsystem>())
				{
					FString Base64Str = FBase64::Encode(PendingImageData);
					Subsystem->AddAttachmentBase64(Base64Str);
				}
			}
			return FReply::Handled(); 
		}
	}
#endif

	return FReply::Unhandled();
}
