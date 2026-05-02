#include "FabServer/Agent/BaseAgent/UmgMcpDefaultChatAgent.h"
#include "Engine/Engine.h"
#if WITH_EDITOR
#include "UmgMcpEditorCompatibility.h"
#endif
// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "FabServer/ChatSystem/UmgMcpSessionManagerSubsystem.h"
#if WITH_EDITOR
#include "Editor.h"
#endif

FUmgMcpDefaultChatAgent::FUmgMcpDefaultChatAgent(const FString& InRelativeJsonPath)
	: FUmgMcpAgent(InRelativeJsonPath)
{
	RestoreConversationContext();
}

void FUmgMcpDefaultChatAgent::RestoreConversationContext()
{
	UUmgMcpSessionManagerSubsystem* SessionSubsystem = nullptr;
#if WITH_EDITOR
	if (GEditor) SessionSubsystem = GEngine->GetEngineSubsystem<UUmgMcpSessionManagerSubsystem>();
#endif
	const FUmgMcpSessionData* ActiveSession = SessionSubsystem ? SessionSubsystem->GetActiveSession() : nullptr;
	if (!ActiveSession)
	{
		return;
	}

	TArray<TSharedPtr<FJsonObject>> RestoredMessages;
	for (const FUmgMcpSessionMessage& SessionMessage : ActiveSession->Messages)
	{
		if (SessionMessage.Role == EUmgMcpSessionRole::User)
		{
			RestoredMessages.Add(CreateMessageObject(TEXT("user"), SessionMessage.Content, SessionMessage.Base64Images));
			continue;
		}

		if (SessionMessage.Role == EUmgMcpSessionRole::Agent)
		{
			// 所有的专家身份（BaseAgent 侧面）共享同一个对话历史流
			RestoredMessages.Add(CreateMessageObject(TEXT("assistant"), SessionMessage.Content));
		}
	}

	if (RestoredMessages.Num() > 0)
	{
		ContextHistory.Append(RestoredMessages);
	}
}
