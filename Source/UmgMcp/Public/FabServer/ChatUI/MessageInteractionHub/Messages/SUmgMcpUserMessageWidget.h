// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

/**
 * SUmgMcpUserMessageWidget
 * 用户消息表现层：实现经典的右侧对齐布局，包含气泡和头像。
 */
class UMGMCP_API SUmgMcpUserMessageWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUmgMcpUserMessageWidget) {}
		SLATE_ARGUMENT(FString, MessageText)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FString MessageText;
};
