// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "DemoTavernHUD.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "LiteRtLmSubsystem.h"

void ADemoTavernHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas) return;

    float YOffset = 100.0f;
    float XOffset = Canvas->SizeX - 350.0f;

    DrawText(TEXT("WINYUNQ LITER_LM PERFORMANCE MONITOR"), FColor::Cyan, XOffset, YOffset - 30.0f, nullptr, 1.2f);
    DrawStatLine(TEXT("ACTIVE AGENT:"), ActiveAgentName, YOffset, FColor::White);
    DrawStatLine(TEXT("SWITCH LATENCY:"), FString::Printf(TEXT("%.2f us"), LastLatencyUs), YOffset, (LastLatencyUs < 1000.0f) ? FColor::Green : FColor::Yellow);
    DrawStatLine(TEXT("THROUGHPUT:"), FString::Printf(TEXT("%.1f tokens/s"), LastTokensPerSec), YOffset, FColor::White);
    
    float VramMB = (float)ULiteRtLmSubsystem::QueryAvailableVramMB();
    DrawStatLine(TEXT("VRAM FOOTPRINT:"), FString::Printf(TEXT("%.1f MB"), VramMB), YOffset, FColor::Orange);

    DrawText(TEXT("STRATEGY BY HUMAN, TACTICS BY AI"), FColor(64, 64, 64), XOffset, YOffset + 20.0f, nullptr, 0.8f);
}

void ADemoTavernHUD::UpdateStats(const FString& InAgentName, float InLatencyUs, float InTokensPerSec)
{
    ActiveAgentName = InAgentName;
    LastLatencyUs = InLatencyUs;
    LastTokensPerSec = InTokensPerSec;
}

void ADemoTavernHUD::DrawStatLine(const FString& Label, const FString& Value, float& YOffset, const FColor& Color)
{
    float XOffset = Canvas->SizeX - 350.0f;
    DrawText(Label, FColor(128, 128, 128), XOffset, YOffset);
    DrawText(Value, Color, XOffset + 150.0f, YOffset);
    YOffset += 25.0f;
}
