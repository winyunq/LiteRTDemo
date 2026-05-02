// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "LiteRtLmUnrealApi.h"
#include "DemoTavernHUD.generated.h"

UCLASS()
class LITERTDEMO_API ADemoTavernHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;
    void UpdateStats(const FString& InAgentName, float InLatencyUs, float InTokensPerSec);

private:
    FString ActiveAgentName = TEXT("None");
    float LastLatencyUs = 0.0f;
    float LastTokensPerSec = 0.0f;

    void DrawStatLine(const FString& Label, const FString& Value, float& YOffset, const FColor& Color);
};
