// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"

class FUmgMcpAgentStack
{
public:
    void Reset(const FString& RootAgentId);
    void Push(const FString& AgentId);
    bool Pop(FString* OutPoppedAgentId = nullptr);

    FString Peek() const;
    int32 Num() const;
    const TArray<FString>& GetAgents() const;

private:
    TArray<FString> AgentIds;
};
