// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "FabServer/ChatUI/TopBar/SUmgMcpUserAvatar.h"
#include "FabServer/UmgMcpStyle.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"

namespace
{
	FText GetLocText(const FString& English, const FString& Chinese)
	{
		const FString Culture = FInternationalization::Get().GetCurrentCulture()->GetName();
		return FText::FromString(Culture.StartsWith(TEXT("zh")) ? Chinese : English);
	}
}

void SUmgMcpUserAvatar::Construct(const FArguments& InArgs)
{
	SourceBrush = FUmgMcpStyle::Get().GetBrush("UmgMcp.Avatar.User");
	SUmgMcpAvatar::Construct(SUmgMcpAvatar::FArguments());
}

TSharedRef<SWidget> SUmgMcpUserAvatar::OnGetMenuContent()
{
	return SNullWidget::NullWidget;
}
