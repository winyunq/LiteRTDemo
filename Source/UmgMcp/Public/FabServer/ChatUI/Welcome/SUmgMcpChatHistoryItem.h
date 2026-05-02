// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

DECLARE_DELEGATE_OneParam(FOnUmgMcpChatHistoryItemClicked, const FString&);

class UMGMCP_API SUmgMcpChatHistoryItem : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUmgMcpChatHistoryItem) {}
		SLATE_ARGUMENT(FString, SessionId)
		SLATE_ARGUMENT(FString, Title)
		SLATE_ARGUMENT(int32, MessageCount)
		SLATE_EVENT(FOnUmgMcpChatHistoryItemClicked, OnClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply HandleClicked();

	FString SessionId;
	FString Title;
	int32 MessageCount = 0;
	FOnUmgMcpChatHistoryItemClicked OnClickedDelegate;
};
