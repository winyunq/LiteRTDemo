// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "FabServer/ChatUI/TopBar/SUmgMcpEditableName.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Styling/AppStyle.h"

void SUmgMcpEditableName::Construct(const FArguments& InArgs)
{
	UserName = InArgs._UserName;
	IsLoggedIn = InArgs._IsLoggedIn;

	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			// 1. 只读文本 (Read-only)
			SNew(STextBlock)
			.Text(this, &SUmgMcpEditableName::GetNameText)
			.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
			.Visibility(this, &SUmgMcpEditableName::GetReadOnlyVisibility)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			// 2. 编辑输入框 (Editable)
			SAssignNew(EditableText, SEditableText)
			.Text(this, &SUmgMcpEditableName::GetNameText)
			.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
			.OnTextCommitted(this, &SUmgMcpEditableName::OnNameCommitted)
			.Visibility(this, &SUmgMcpEditableName::GetEditableVisibility)
			.MinDesiredWidth(50.0f)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4.0f, 0.0f, 0.0f, 0.0f)
		[
			// 3. 编辑按钮 (Edit Button) - 只有非登录态可见
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.OnClicked(this, &SUmgMcpEditableName::OnEditClicked)
			.Visibility(this, &SUmgMcpEditableName::GetEditButtonVisibility)
			.ToolTipText(FText::FromString(TEXT("Edit Name")))
			[

					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.Edit"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				
			]
		]
	];
}

FText SUmgMcpEditableName::GetNameText() const
{
	return FText::FromString(UserName.Get());
}

EVisibility SUmgMcpEditableName::GetReadOnlyVisibility() const
{
	// 登录态始终只读；非登录态下根据 bIsEditing 决定
	if (IsLoggedIn.Get()) return EVisibility::Visible;
	return bIsEditing ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SUmgMcpEditableName::GetEditableVisibility() const
{
	if (IsLoggedIn.Get()) return EVisibility::Collapsed;
	return bIsEditing ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SUmgMcpEditableName::GetEditButtonVisibility() const
{
	// 只有未登录且不在编辑中时，才显示编辑按钮
	return (!IsLoggedIn.Get() && !bIsEditing) ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply SUmgMcpEditableName::OnEditClicked()
{
	bIsEditing = true;
	return FReply::Handled();
}

void SUmgMcpEditableName::OnNameCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter || CommitType == ETextCommit::OnUserMovedFocus)
	{
		// 模拟保存逻辑（实际应通过委托传回）
		bIsEditing = false;
	}
	else if (CommitType == ETextCommit::Default)
	{
		bIsEditing = false;
	}
}
