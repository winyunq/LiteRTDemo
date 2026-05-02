// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "FabServer/ChatUI/BottomBar/SBottomBar.h"

#include "FabServer/ChatUI/BottomBar/SUmgMcpChatInput.h"
#include "FabServer/ChatUI/BottomBar/SUmgMcpChatSendButton.h"
#include "FabServer/ChatUI/BottomBar/SUmgMcpAttachmentList.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformApplicationMisc.h"
#include "FabServer/ChatUI/BottomBar/SUmgMcpQuotaBar.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SSpacer.h"

void SBottomBar::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)
		
				// 0. 附件列表
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			SAssignNew(AttachmentList, SUmgMcpAttachmentList)
		]

		// 1. 输入框
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(ChatInput, SUmgMcpChatInput)
		]

		// 2. 控制工具条 (原子控件拼装)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 5.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SSpacer)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew(SendButton, SUmgMcpChatSendButton)
			]
		]

		// 3. 配额进度条
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 5.0f, 0.0f, 0.0f)
		[
			SAssignNew(QuotaBar, SUmgMcpQuotaBar)
			.Percent(0.5f)
			.Visibility(EVisibility::Collapsed) 
		]
	];
}

void SBottomBar::OnInteractionModeChanged(const FString& NewMode)
{
}

void SBottomBar::OnChatInputSendRequested()
{
}

void SBottomBar::OnChatInputPasteImage(const TArray<uint8>& ImageData, int32 Width, int32 Height)
{
}

void SBottomBar::OnToolModeChanged(const FString& NewTool)
{
}
FReply SBottomBar::OnAddAttachmentClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform) return FReply::Handled();

	TArray<FString> OutFiles;
	void* ParentWindowHandle = FSlateApplication::Get().GetActiveTopLevelWindow()->GetNativeWindow()->GetOSWindowHandle();

	if (DesktopPlatform->OpenFileDialog(
		ParentWindowHandle,
		TEXT("Select Image Attachment"),
		TEXT(""),
		TEXT(""),
		TEXT("Image Files (*.png;*.jpg;*.jpeg)|*.png;*.jpg;*.jpeg"),
		EFileDialogFlags::None,
		OutFiles
	))
	{
		for (const FString& FilePath : OutFiles)
		{
			TArray<uint8> FileData;
			if (FFileHelper::LoadFileToArray(FileData, *FilePath))
			{
				FString Base64Str = FBase64::Encode(FileData);
				if (AttachmentList.IsValid())
				{
					AttachmentList->AddAttachment(Base64Str);
				}
			}
		}
	}

	return FReply::Handled();
}
