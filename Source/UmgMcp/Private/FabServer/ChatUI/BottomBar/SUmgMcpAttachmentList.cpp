#include "FabServer/ChatUI/BottomBar/SUmgMcpAttachmentList.h"
#include "Engine/Engine.h"
// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "FabServer/ChatSystem/UmgMcpActiveMessageSubsystem.h"
#if WITH_EDITOR
#include "Editor.h"
#endif

void SUmgMcpAttachmentList::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SAssignNew(ListContainer, SHorizontalBox)
	];

	if (GEngine)
	{
		if (auto* Subsystem = GEngine->GetEngineSubsystem<UUmgMcpActiveMessageSubsystem>())
		{
			Subsystem->RegisterAttachmentList(SharedThis(this));
		}
	}
}

TArray<FString> SUmgMcpAttachmentList::GetBase64Images() const
{
	return AttachedImages;
}

void SUmgMcpAttachmentList::ClearAttachments()
{
	AttachedImages.Empty();
	RefreshList();
}

void SUmgMcpAttachmentList::AddAttachment(const FString& InBase64)
{
	AttachedImages.Add(InBase64);
	RefreshList();
}

void SUmgMcpAttachmentList::RefreshList()
{
	ListContainer->ClearChildren();

	for (int32 i = 0; i < AttachedImages.Num(); ++i)
	{
		// 简单的占位缩略图逻辑 (真正的图片解码显示后续完善)
		ListContainer->AddSlot()
		.AutoWidth()
		.Padding(5.0f, 0.0f)
		[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.Image")) // 默认显示图片占位图标
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Top)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
					.OnClicked_Lambda([this, i]() {
						this->AttachedImages.RemoveAt(i);
						this->RefreshList();
						return FReply::Handled();
					})
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("×")))
						.ColorAndOpacity(FLinearColor::Red)
					]
				]
			
		];
	}

	// 如果没有附件，则隐藏容器
	SetVisibility(AttachedImages.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed);
}
