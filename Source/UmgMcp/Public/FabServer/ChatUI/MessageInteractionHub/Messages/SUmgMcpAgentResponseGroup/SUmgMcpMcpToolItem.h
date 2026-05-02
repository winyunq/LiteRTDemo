// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

/**
 * SUmgMcpMcpToolItem: 单个 MCP 工具请求的 UI 项。
 * 负责展示工具名、参数详情及执行状态。
 */
class UMGMCP_API SUmgMcpMcpToolItem : public SCompoundWidget
{
public:
	enum class EToolStatus
	{
		Pending,
		Running,
		Success,
		Failed,
		Rejected
	};

	SLATE_BEGIN_ARGS(SUmgMcpMcpToolItem) {}
		SLATE_ARGUMENT(FString, ToolName)
		SLATE_ARGUMENT(FString, ArgumentsJson)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** 更新当前工具的执行状态 */
	void SetStatus(EToolStatus InStatus, const FString& ErrorMessage = TEXT(""));

	/** 获取工具名 */
	FString GetToolName() const { return ToolName; }
	/** 获取参数 JSON */
	FString GetArgumentsJson() const { return ArgumentsJson; }

private:
	FString ToolName;
	FString ArgumentsJson;
	EToolStatus CurrentStatus = EToolStatus::Pending;

	TSharedPtr<class STextBlock> StatusTextBlock;
	TSharedPtr<class SVerticalBox> DetailsBox;
};
