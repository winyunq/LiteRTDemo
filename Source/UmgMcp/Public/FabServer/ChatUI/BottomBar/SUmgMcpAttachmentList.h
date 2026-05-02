// Copyright (c) 2025-2026 Winyunq. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

/**
 * SUmgMcpAttachmentList
 * 附件列表控件：用于展示和管理当前消息准备发送的图片实质。
 */
class UMGMCP_API SUmgMcpAttachmentList : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUmgMcpAttachmentList) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** 获取当前所有附件的 Base64 数据 */
	TArray<FString> GetBase64Images() const;

	/** 清空附件 */
	void ClearAttachments();

	/** 手动添加附件 (Base64) */
	void AddAttachment(const FString& InBase64);

private:
	/** 刷新 UI */
	void RefreshList();

	/** 附件数据列表 (Base64 字符串) */
	TArray<FString> AttachedImages;

	/** 列表容器 */
	TSharedPtr<SHorizontalBox> ListContainer;
};
