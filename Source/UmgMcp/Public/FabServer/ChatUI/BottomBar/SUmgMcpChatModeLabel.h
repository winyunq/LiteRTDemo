// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class UMGMCP_API SUmgMcpChatModeLabel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUmgMcpChatModeLabel) {}
		SLATE_ATTRIBUTE(FString, ModeText)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FText GetModeDisplayText() const;
	FText GetLocText(const FString& English, const FString& Chinese) const;

	TAttribute<FString> ModeText;
};
