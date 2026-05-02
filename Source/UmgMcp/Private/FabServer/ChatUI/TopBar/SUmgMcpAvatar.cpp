// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "FabServer/ChatUI/TopBar/SUmgMcpAvatar.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/AppStyle.h"
#include "FabServer/UmgMcpStyle.h"
#include "Styling/StyleColors.h"

void SUmgMcpAvatar::Construct(const FArguments& InArgs)
{
	if (!SourceBrush)
	{
		SourceBrush = FUmgMcpStyle::Get().GetBrush("UmgMcp.ChatAvatar");
	}

	ChildSlot
	[
		SAssignNew(ComboButton, SComboButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.HasDownArrow(false)
		.ContentPadding(0.0f)
		.OnGetMenuContent(this, &SUmgMcpAvatar::OnGetMenuContent)
		.ButtonContent()
		[
			SNew(SBox)
			.WidthOverride(AvatarSize)
			.HeightOverride(AvatarSize)
			[
				SNew(SImage)
				.Image(this, &SUmgMcpAvatar::GetRoundedAvatarBrush)
			]
		]
	];
}


const FSlateBrush* SUmgMcpAvatar::GetRoundedAvatarBrush() const
{
	if (!SourceBrush) return FAppStyle::Get().GetBrush("Icons.User");

	if (!CachedRoundedAvatarBrush.IsValid())
	{
		CachedRoundedAvatarBrush = MakeShared<FSlateRoundedBoxBrush>(
			SourceBrush->GetResourceName(),
			FLinearColor::White,
			FVector4(64.0f, 64.0f, 64.0f, 64.0f), // 128 / 2 = 完美的圆
			FLinearColor::Transparent,
			0.0f,
			FVector2D(128.0f, 128.0f)
		);
	}

	return CachedRoundedAvatarBrush.Get();
}
