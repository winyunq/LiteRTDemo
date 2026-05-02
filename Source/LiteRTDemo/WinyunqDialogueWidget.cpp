// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "WinyunqDialogueWidget.h"
#include "Components/EditableText.h"
#include "Components/TextBlock.h"
#include "LiteRtLmUnrealApi.h"
#include "LiteRtLmSubsystem.h"
#include "Engine/Engine.h"

void UWinyunqDialogueWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Input_UserMessage)
    {
        Input_UserMessage->OnTextCommitted.AddDynamic(this, &UWinyunqDialogueWidget::OnUserTextCommitted);
    }
}

void UWinyunqDialogueWidget::SetTargetNPC(ULiteRtLmComponent* InNPC)
{
    if (CurrentNPC)
    {
        CurrentNPC->OnTextChunkReceived.RemoveAll(this);
        CurrentNPC->OnInferenceCompleted.RemoveAll(this);
    }

    CurrentNPC = InNPC;

    if (CurrentNPC)
    {
        CurrentNPC->OnTextChunkReceived.AddDynamic(this, &UWinyunqDialogueWidget::OnChunkReceived);
        CurrentNPC->OnInferenceCompleted.AddDynamic(this, &UWinyunqDialogueWidget::OnInferenceDone);
    }
}

void UWinyunqDialogueWidget::OnUserTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
    if (CommitMethod == ETextCommit::OnEnter && !Text.IsEmpty() && CurrentNPC)
    {
        FString UserMsg = Text.ToString();
        
        // Update History
        FString CurrentHistory = Text_DialogueHistory->GetText().ToString();
        Text_DialogueHistory->SetText(FText::FromString(CurrentHistory + "\nUSER: " + UserMsg + "\nAI: "));
        
        AccumulatedResponse = "";
        CurrentNPC->SendChatMessage(UserMsg);
        
        Input_UserMessage->SetText(FText::GetEmpty());
    }
}

void UWinyunqDialogueWidget::OnChunkReceived(const FString& Chunk)
{
    AccumulatedResponse += Chunk;
    
    // Simple update - for production, use a more efficient rich text buffer
    FString CurrentHistory = Text_DialogueHistory->GetText().ToString();
    Text_DialogueHistory->SetText(FText::FromString(CurrentHistory + Chunk));
}

void UWinyunqDialogueWidget::OnInferenceDone(const FLiteRtLmResult& Result)
{
    if (Text_Stat_Latency)
    {
        Text_Stat_Latency->SetText(FText::FromString(FString::Printf(TEXT("Latency: %.2f us"), Result.TimeMs * 1000.0f))); // Assuming Result.TimeMs is ms
    }

    if (Text_Stat_VRAM)
    {
        float Vram = 0.0f;
        if (ULiteRtLmSubsystem* Subsystem = GEngine->GetEngineSubsystem<ULiteRtLmSubsystem>())
        {
            Vram = Subsystem->QueryAvailableVramMB();
        }
        Text_Stat_VRAM->SetText(FText::FromString(FString::Printf(TEXT("VRAM: %.1f MB"), Vram)));
    }
}
