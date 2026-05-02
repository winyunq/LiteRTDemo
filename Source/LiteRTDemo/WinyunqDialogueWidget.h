// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LiteRtLmComponent.h"
#include "WinyunqDialogueWidget.generated.h"

class UEditableText;
class UTextBlock;
class UScrollBox;

/**
 * C++ Driven UMG Widget for LiteRT-LM Interaction.
 */
UCLASS()
class LITERTDEMO_API UWinyunqDialogueWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Target NPC component this UI is currently talking to. */
    void SetTargetNPC(ULiteRtLmComponent* InNPC);

protected:
    virtual void NativeConstruct() override;

    // UI Bindings (Names must match in Blueprint or we create them in C++)
    UPROPERTY(meta = (BindWidget))
    UTextBlock* Text_DialogueHistory;

    UPROPERTY(meta = (BindWidget))
    UEditableText* Input_UserMessage;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* Text_Stat_Latency;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* Text_Stat_VRAM;

    UFUNCTION()
    void OnUserTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

    UFUNCTION()
    void OnChunkReceived(const FString& Chunk);

    UFUNCTION()
    void OnInferenceDone(const FLiteRtLmResult& Result);

private:
    UPROPERTY()
    ULiteRtLmComponent* CurrentNPC = nullptr;

    FString AccumulatedResponse;
};
