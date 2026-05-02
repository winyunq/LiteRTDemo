// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "FabServer/Agent/UmgMcpAgentStack.h"

void FUmgMcpAgentStack::Reset(const FString& RootAgentId)
{
    AgentIds.Empty();
    if (!RootAgentId.IsEmpty())
    {
        AgentIds.Add(RootAgentId);
    }
}

void FUmgMcpAgentStack::Push(const FString& AgentId)
{
    if (!AgentId.IsEmpty())
    {
        AgentIds.Add(AgentId);
    }
}

bool FUmgMcpAgentStack::Pop(FString* OutPoppedAgentId)
{
    if (AgentIds.Num() <= 1)
    {
        return false;
    }

    const FString Popped = AgentIds.Pop(EAllowShrinking::No);
    if (OutPoppedAgentId)
    {
        *OutPoppedAgentId = Popped;
    }
    return true;
}

FString FUmgMcpAgentStack::Peek() const
{
    return AgentIds.Num() > 0 ? AgentIds.Last() : FString();
}

int32 FUmgMcpAgentStack::Num() const
{
    return AgentIds.Num();
}

const TArray<FString>& FUmgMcpAgentStack::GetAgents() const
{
    return AgentIds;
}
