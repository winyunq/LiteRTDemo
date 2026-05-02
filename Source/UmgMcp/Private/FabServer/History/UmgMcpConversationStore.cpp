// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "FabServer/History/UmgMcpConversationStore.h"
#include "FabServer/History/UmgMcpConversationHistoryNormalizer.h"
#include "Misc/ScopeLock.h"

FCriticalSection FUmgMcpConversationStore::ConversationHistoryCs;
FCriticalSection FUmgMcpConversationStore::SystemInstructionCs;
TArray<TSharedPtr<FJsonObject>> FUmgMcpConversationStore::ConversationHistory;
FString FUmgMcpConversationStore::DynamicSystemInstruction;

void FUmgMcpConversationStore::Clear()
{
    FScopeLock ScopeLock(&ConversationHistoryCs);
    ConversationHistory.Empty();
}

TArray<TSharedPtr<FJsonObject>>& FUmgMcpConversationStore::GetMutableHistory()
{
    return ConversationHistory;
}

TArray<TSharedPtr<FJsonObject>> FUmgMcpConversationStore::GetSnapshot()
{
    FScopeLock ScopeLock(&ConversationHistoryCs);
    return ConversationHistory;
}

void FUmgMcpConversationStore::Replace(const TArray<TSharedPtr<FJsonObject>>& NewHistory)
{
    FScopeLock ScopeLock(&ConversationHistoryCs);
    ConversationHistory = NewHistory;
}

TArray<TSharedPtr<FJsonObject>> FUmgMcpConversationStore::Normalize()
{
    FScopeLock ScopeLock(&ConversationHistoryCs);
    ConversationHistory = FUmgMcpConversationHistoryNormalizer::Normalize(ConversationHistory);
    return ConversationHistory;
}

void FUmgMcpConversationStore::SetRuntimeSystemInstruction(const FString& Instruction)
{
    FScopeLock ScopeLock(&SystemInstructionCs);
    DynamicSystemInstruction = Instruction;
}

void FUmgMcpConversationStore::ClearRuntimeSystemInstruction()
{
    FScopeLock ScopeLock(&SystemInstructionCs);
    DynamicSystemInstruction.Empty();
}

FString FUmgMcpConversationStore::GetRuntimeSystemInstruction()
{
    FScopeLock ScopeLock(&SystemInstructionCs);
    return DynamicSystemInstruction;
}
