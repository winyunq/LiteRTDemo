// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

DECLARE_DELEGATE_ThreeParams(FOnPasteImage, const TArray<uint8>& /*ImageData*/, int32 /*Width*/, int32 /*Height*/);

/**
 * SUmgMcpChatInput：原子控件，仅负责多行文本输入。
 * 模式选择、发送按钮等已被剥离到 BottomBar 的其他原子控件中。
 */
class UMGMCP_API SUmgMcpChatInput : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUmgMcpChatInput) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	FText GetText() const;
	void SetText(const FText& InText);
	void ClearText();

public:
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

private:
	FReply OnInputKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);

	TSharedPtr<class SMultiLineEditableTextBox> InputTextBox;
};
