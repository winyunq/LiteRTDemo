// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "FabServer/ChatUI/Welcome/SUmgMcpChatWelcomeHeader.h"

#include "Styling/AppStyle.h"
#include "Widgets/Text/STextBlock.h"

void SUmgMcpChatWelcomeHeader::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("Unreal Motion Graphics AI Assistant")))
		.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
	];
}
