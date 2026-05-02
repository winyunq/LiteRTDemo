// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "HAL/CriticalSection.h"

/**
 * Thread-safe storage for FabServer conversation history and runtime system instructions.
 */
class UMGMCP_API FUmgMcpConversationStore
{
public:
    /** Remove all stored history entries. */
    static void Clear();

    /** Mutable access to the shared history buffer. */
    static TArray<TSharedPtr<FJsonObject>>& GetMutableHistory();

    /** Snapshot of the current history (copied under a lock). */
    static TArray<TSharedPtr<FJsonObject>> GetSnapshot();

    /** Replace the stored history with a new array. */
    static void Replace(const TArray<TSharedPtr<FJsonObject>>& NewHistory);

    /** Normalize stored history using the standard normalizer. */
    static TArray<TSharedPtr<FJsonObject>> Normalize();

    /** Set or clear a one-off system instruction appended at schema build time. */
    static void SetRuntimeSystemInstruction(const FString& Instruction);
    static void ClearRuntimeSystemInstruction();
    static FString GetRuntimeSystemInstruction();

private:
    static FCriticalSection ConversationHistoryCs;
    static FCriticalSection SystemInstructionCs;
    static TArray<TSharedPtr<FJsonObject>> ConversationHistory;
    static FString DynamicSystemInstruction;
};
