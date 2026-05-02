// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

// 进度条原子控件
class UMGMCP_API SUmgMcpQuotaBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUmgMcpQuotaBar) {}
		SLATE_ATTRIBUTE(TOptional<float>, Percent)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FText GetQuotaText() const;

	// 修复：更改为 TOptional 类型以匹配 SProgressBar
	TAttribute<TOptional<float>> Percent;
};
