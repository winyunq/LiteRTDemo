// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "FabServer/Agent/UmgMcpAgent.h"

/**
 * FUmgMcpDefaultChatAgent
 * Default context-aware chat agent used by BaseAgent modes.
 * It restores its runtime context from the active session history.
 */
class UMGMCP_API FUmgMcpDefaultChatAgent : public FUmgMcpAgent
{
public:
	explicit FUmgMcpDefaultChatAgent(const FString& InRelativeJsonPath);
	virtual ~FUmgMcpDefaultChatAgent() = default;

	void RestoreConversationContext();
};
