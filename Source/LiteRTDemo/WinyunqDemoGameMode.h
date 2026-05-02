// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WinyunqDemoGameMode.generated.h"

/**
 * GameMode for the LiteRT-LM Demo.
 * Automatically initializes the HUD and UI.
 */
UCLASS()
class LITERTDEMO_API AWinyunqDemoGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
    AWinyunqDemoGameMode();

protected:
    virtual void BeginPlay() override;
};
