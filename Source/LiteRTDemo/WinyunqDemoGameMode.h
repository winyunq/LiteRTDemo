// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LiteRtLmModelDownloader.h"
#include "WinyunqDemoGameMode.generated.h"

class AActor;
class UButton;
class UTextBlock;
class UUserWidget;
class ULiteRtLmDownloadModelAsyncAction;

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

private:
    UPROPERTY()
    TSubclassOf<UUserWidget> AIWerewolfWidgetClass;

    UPROPERTY()
    TSubclassOf<AActor> AIWerewolfDirectorClass;

    UPROPERTY()
    TObjectPtr<UUserWidget> ActiveAIWerewolfWidget;

    UPROPERTY()
    TObjectPtr<AActor> ActiveAIWerewolfDirector;

    UPROPERTY()
    TObjectPtr<ULiteRtLmDownloadModelAsyncAction> ActiveModelDownload;

    int32 SelectedPlayerCount = 6;

    FTimerHandle DeferredLoadModelTimerHandle;

    void BindAIWerewolfSetupButtons(UUserWidget* Widget);
    UButton* FindButton(FName WidgetName) const;
    UTextBlock* FindTextBlock(FName WidgetName) const;
    void SetTextBlock(FName WidgetName, const FString& Text);
    void SetModelStatus(const FString& Text);
    void SetDownloadProgress(const FString& Text);
    void SetGameLog(const FString& Text);
    void SetSelectedPlayerCount(int32 Count);
    void LoadDownloadedModelDeferred();
    AActor* GetOrCreateAIWerewolfDirector();
    void ExecuteDirectorFunction(FName FunctionName, const FString& ActionLabel);
    void ApplySelectedPlayerCountToObject(UObject* Target) const;
    void SetWidgetModelReady(bool bReady) const;

    UFUNCTION()
    void HandleDownloadE2BClicked();

    UFUNCTION()
    void HandleLoadDownloadedClicked();

    UFUNCTION()
    void HandleImportModelClicked();

    UFUNCTION()
    void HandleStartGameClicked();

    UFUNCTION()
    void HandleAutoTestClicked();

    UFUNCTION()
    void HandleResetGameClicked();

    UFUNCTION()
    void HandlePlayer5Clicked();

    UFUNCTION()
    void HandlePlayer6Clicked();

    UFUNCTION()
    void HandlePlayer7Clicked();

    UFUNCTION()
    void HandlePlayer8Clicked();

    UFUNCTION()
    void HandlePlayer9Clicked();

    UFUNCTION()
    void HandlePlayer10Clicked();

    UFUNCTION()
    void HandleModelDownloadProgress(int64 BytesReceived, int64 ContentLength, float Progress);

    UFUNCTION()
    void HandleModelDownloadCompleted(const FLiteRtLmModelDownloadResult& Result);
};
