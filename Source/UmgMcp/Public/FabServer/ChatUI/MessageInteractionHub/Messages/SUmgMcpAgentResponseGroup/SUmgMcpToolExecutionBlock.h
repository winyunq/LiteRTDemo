// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UMGMCP_API SUmgMcpToolExecutionBlock : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUmgMcpToolExecutionBlock) {}
		SLATE_ARGUMENT(FString, ToolName)
		SLATE_ARGUMENT(FString, Status)
		SLATE_ARGUMENT(bool, IsError)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};
