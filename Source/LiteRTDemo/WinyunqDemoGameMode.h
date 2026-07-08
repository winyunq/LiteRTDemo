// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LiteRtLmModelDownloader.h"
#include "LiteRtLmUnrealApi.h"
#include "WinyunqDemoGameMode.generated.h"

class AActor;
class UButton;
class UCanvasPanel;
class UEditableTextBox;
class UHorizontalBox;
class UScrollBox;
class UTextBlock;
class UUserWidget;
class UVerticalBox;
class ULiteRtLmDownloadModelAsyncAction;

enum class EAIWerewolfRuntimePhase : uint8
{
    Setup,
    Night,
    Discussion,
    Voting,
    Results,
    Ended
};

enum class EAIWerewolfRuntimeAIRequest : uint8
{
    None,
    NightKill,
    DiscussionSpeech,
    Vote
};

struct FAIWerewolfRuntimePlayer
{
    FString Name;
    FString Role;
    FLinearColor AvatarColor = FLinearColor::White;
    bool bAlive = true;
    bool bHuman = false;

    bool IsWerewolf() const
    {
        return Role.Equals(TEXT("Werewolf"), ESearchCase::IgnoreCase);
    }
};

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

    UPROPERTY()
    TObjectPtr<UCanvasPanel> RuntimeGamePanel;

    UPROPERTY()
    TObjectPtr<UTextBlock> RuntimePhaseText;

    UPROPERTY()
    TObjectPtr<UTextBlock> RuntimeInstructionText;

    UPROPERTY()
    TObjectPtr<UTextBlock> RuntimeNextPhaseButtonText;

    UPROPERTY()
    TObjectPtr<UButton> RuntimeNextPhaseButton;

    UPROPERTY()
    TObjectPtr<UScrollBox> RuntimeChatScrollBox;

    UPROPERTY()
    TObjectPtr<UEditableTextBox> RuntimePlayerInput;

    UPROPERTY()
    TObjectPtr<UVerticalBox> RuntimeVoteBox;

    UPROPERTY()
    TArray<TObjectPtr<UButton>> RuntimePlayerButtons;

    UPROPERTY()
    TArray<TObjectPtr<UTextBlock>> RuntimePlayerAvatarTexts;

    UPROPERTY()
    TArray<TObjectPtr<UTextBlock>> RuntimePlayerNameTexts;

    UPROPERTY()
    TArray<TObjectPtr<UTextBlock>> RuntimePlayerRoleTexts;

    UPROPERTY()
    TArray<TObjectPtr<UTextBlock>> RuntimePlayerStatusTexts;

    int32 SelectedPlayerCount = 6;
    int32 RuntimeRoundIndex = 0;
    int32 HumanVoteTarget = INDEX_NONE;
    int32 RuntimeActiveAIPlayerIndex = INDEX_NONE;
    EAIWerewolfRuntimePhase RuntimePhase = EAIWerewolfRuntimePhase::Setup;
    EAIWerewolfRuntimeAIRequest RuntimeAIRequestType = EAIWerewolfRuntimeAIRequest::None;
    bool bRuntimeAIRequestInFlight = false;
    FString RuntimeActiveAIResponse;
    TArray<FAIWerewolfRuntimePlayer> RuntimePlayers;
    TArray<int32> RuntimePendingAIPlayers;
    TMap<int32, int32> RuntimeVoteCounts;

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
    void BuildRuntimeWerewolfGameUI();
    void ShowRuntimeWerewolfGameUI(bool bShow);
    void StartRuntimeWerewolfGame();
    void ResetRuntimeWerewolfGame();
    void ReturnToSetupUI();
    void AdvanceRuntimeWerewolfPhase();
    void RunRuntimeNightPhase();
    void CompleteRuntimeNightPhaseFromAI(const FString& AIText);
    void RunRuntimeDiscussionPhase();
    void StartNextRuntimeDiscussionAI();
    void CompleteRuntimeDiscussionAI(const FString& AIText);
    void EnterRuntimeVotingPhase();
    void ResolveRuntimeVote();
    void StartNextRuntimeVoteAI();
    void CompleteRuntimeVoteAI(const FString& AIText);
    void FinishRuntimeVoteResolution();
    bool EvaluateRuntimeWinCondition();
    bool StartRuntimeAIRequest(EAIWerewolfRuntimeAIRequest RequestType, int32 ActorPlayerIndex, const FString& UserPrompt);
    FString BuildRuntimeGameStateText() const;
    FString BuildRuntimeNightPrompt(int32 ActorPlayerIndex) const;
    FString BuildRuntimeDiscussionPrompt(int32 ActorPlayerIndex) const;
    FString BuildRuntimeVotePrompt(int32 ActorPlayerIndex) const;
    int32 ParseRuntimeTargetIndexFromText(const FString& Text) const;
    bool IsRuntimeVoteTargetValid(int32 TargetIndex, int32 VoterIndex) const;
    void HandleRuntimeVoteClicked(int32 PlayerIndex);
    void RebuildRuntimeVoteButtons();
    void RefreshRuntimePlayers();
    void RefreshRuntimePhaseText();
    void AppendRuntimeChatMessage(const FString& Speaker, const FString& Message, const FLinearColor& AccentColor);
    FString BuildRuntimeAISpeech(int32 PlayerIndex) const;
    FString GetRuntimeRoleDisplay(const FAIWerewolfRuntimePlayer& Player) const;
    int32 ChooseRuntimeNightTarget() const;
    int32 ChooseRuntimeVoteTarget(int32 VoterIndex) const;
    TArray<int32> GetAliveRuntimePlayerIndices() const;
    UTextBlock* CreateRuntimeText(FName WidgetName, const FString& Text, int32 FontSize, const FLinearColor& Color, bool bAutoWrap = false) const;
    UButton* CreateRuntimeButton(FName WidgetName, const FString& Label, FName HandlerName, const FLinearColor& ButtonColor, UTextBlock** OutLabelText = nullptr);
    void BindRuntimeButton(UButton* Button, FName HandlerName);

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

    UFUNCTION()
    void HandleRuntimeSendClicked();

    UFUNCTION()
    void HandleRuntimeNextPhaseClicked();

    UFUNCTION()
    void HandleRuntimeResetClicked();

    UFUNCTION()
    void HandleRuntimeBackSetupClicked();

    UFUNCTION()
    void HandleRuntimeVotePlayer1Clicked();

    UFUNCTION()
    void HandleRuntimeVotePlayer2Clicked();

    UFUNCTION()
    void HandleRuntimeVotePlayer3Clicked();

    UFUNCTION()
    void HandleRuntimeVotePlayer4Clicked();

    UFUNCTION()
    void HandleRuntimeVotePlayer5Clicked();

    UFUNCTION()
    void HandleRuntimeVotePlayer6Clicked();

    UFUNCTION()
    void HandleRuntimeVotePlayer7Clicked();

    UFUNCTION()
    void HandleRuntimeVotePlayer8Clicked();

    UFUNCTION()
    void HandleRuntimeVotePlayer9Clicked();

    UFUNCTION()
    void HandleRuntimeVotePlayer10Clicked();

    UFUNCTION()
    void HandleRuntimeAIChunk(const FString& TextChunk);

    UFUNCTION()
    void HandleRuntimeAIDone(const FLiteRtLmResult& Result);
};
