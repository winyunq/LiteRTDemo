// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "FabServer/ChatUI/TopBar/SConfigByAuthentication.h"

#include "FabServer/AIProviders/UmgMcpAiSubsystem.h"
#if WITH_EDITOR
#include "Editor.h"
#endif
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"

void SConfigByAuthentication::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("Provider")))
	];
}
