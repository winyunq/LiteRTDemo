#include "WinyunqDemoGameMode.h"
#include "DemoTavernHUD.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "LiteRtLmBlueprintLibrary.h"
#include "LiteRtLmModelDownloader.h"
#include "TimerManager.h"
#include "WinyunqMcpWrapper.h"
#include "Blueprint/UserWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

namespace
{
constexpr TCHAR Gemma4E2BModelFileName[] = TEXT("gemma-4-E2B-it.litertlm");
constexpr double BytesPerMegabyte = 1024.0 * 1024.0;

template <typename T>
bool SetTypedPropertyValue(UObject* Target, FName PropertyName, const T& Value)
{
    return false;
}

template <>
bool SetTypedPropertyValue<int32>(UObject* Target, FName PropertyName, const int32& Value)
{
    if (!Target)
    {
        return false;
    }

    if (FIntProperty* Property = FindFProperty<FIntProperty>(Target->GetClass(), PropertyName))
    {
        Property->SetPropertyValue_InContainer(Target, Value);
        return true;
    }

    return false;
}

template <>
bool SetTypedPropertyValue<bool>(UObject* Target, FName PropertyName, const bool& Value)
{
    if (!Target)
    {
        return false;
    }

    if (FBoolProperty* Property = FindFProperty<FBoolProperty>(Target->GetClass(), PropertyName))
    {
        Property->SetPropertyValue_InContainer(Target, Value);
        return true;
    }

    return false;
}
}

AWinyunqDemoGameMode::AWinyunqDemoGameMode()
{
    HUDClass = ADemoTavernHUD::StaticClass();

    static ConstructorHelpers::FClassFinder<UUserWidget> AIWerewolfWidgetFinder(
        TEXT("/Game/AIWerewolf/WBP_AIWerewolfGame"));
    if (AIWerewolfWidgetFinder.Succeeded())
    {
        AIWerewolfWidgetClass = AIWerewolfWidgetFinder.Class;
    }

    static ConstructorHelpers::FClassFinder<AActor> AIWerewolfDirectorFinder(
        TEXT("/Game/AIWerewolf/BP_AIWerewolfDirector"));
    if (AIWerewolfDirectorFinder.Succeeded())
    {
        AIWerewolfDirectorClass = AIWerewolfDirectorFinder.Class;
    }
}

void AWinyunqDemoGameMode::BeginPlay()
{
    Super::BeginPlay();

    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        UUserWidget* MainUI = nullptr;
        if (AIWerewolfWidgetClass)
        {
            MainUI = CreateWidget<UUserWidget>(PC, AIWerewolfWidgetClass);
        }

        if (!MainUI)
        {
            MainUI = CreateWidget<UWinyunqMcpWrapper>(PC, UWinyunqMcpWrapper::StaticClass());
        }

        if (MainUI)
        {
            ActiveAIWerewolfWidget = MainUI;
            MainUI->AddToViewport();

            FInputModeGameAndUI InputMode;
            InputMode.SetWidgetToFocus(MainUI->TakeWidget());
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            InputMode.SetHideCursorDuringCapture(false);

            PC->bShowMouseCursor = true;
            PC->SetInputMode(InputMode);

            BindAIWerewolfSetupButtons(MainUI);
        }
    }
}

void AWinyunqDemoGameMode::BindAIWerewolfSetupButtons(UUserWidget* Widget)
{
    ActiveAIWerewolfWidget = Widget;

    int32 BoundButtonCount = 0;
    TArray<FString> MissingButtons;

    auto BindButton = [this, &BoundButtonCount, &MissingButtons](FName ButtonName, FName HandlerName)
    {
        if (UButton* Button = FindButton(ButtonName))
        {
            Button->OnClicked.Clear();
            FScriptDelegate Delegate;
            Delegate.BindUFunction(this, HandlerName);
            Button->OnClicked.AddUnique(Delegate);
            Button->SetIsEnabled(true);
            ++BoundButtonCount;
        }
        else
        {
            MissingButtons.Add(ButtonName.ToString());
        }
    };

    BindButton(TEXT("DownloadE2BButton"), GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleDownloadE2BClicked));
    BindButton(TEXT("LoadDownloadedButton"), GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleLoadDownloadedClicked));
    BindButton(TEXT("ImportModelButton"), GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleImportModelClicked));
    BindButton(TEXT("StartGameButton"), GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleStartGameClicked));
    BindButton(TEXT("AutoTestButton"), GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleAutoTestClicked));
    BindButton(TEXT("ResetGameButton"), GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleResetGameClicked));
    BindButton(TEXT("Player5Button"), GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandlePlayer5Clicked));
    BindButton(TEXT("Player6Button"), GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandlePlayer6Clicked));
    BindButton(TEXT("Player7Button"), GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandlePlayer7Clicked));
    BindButton(TEXT("Player8Button"), GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandlePlayer8Clicked));
    BindButton(TEXT("Player9Button"), GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandlePlayer9Clicked));
    BindButton(TEXT("Player10Button"), GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandlePlayer10Clicked));

    SetSelectedPlayerCount(SelectedPlayerCount);

    const FString MissingSummary = MissingButtons.Num() > 0
        ? FString::Printf(TEXT(" Missing: %s."), *FString::Join(MissingButtons, TEXT(", ")))
        : FString();

    SetModelStatus(TEXT("Model status / 模型状态: ready for download or load."));
    SetDownloadProgress(TEXT("Download progress / 下载进度: idle."));
    SetGameLog(FString::Printf(
        TEXT("Setup controls bound at runtime / 设置按钮已在运行时绑定: %d buttons.%s"),
        BoundButtonCount,
        *MissingSummary));
}

UButton* AWinyunqDemoGameMode::FindButton(FName WidgetName) const
{
    return ActiveAIWerewolfWidget
        ? Cast<UButton>(ActiveAIWerewolfWidget->GetWidgetFromName(WidgetName))
        : nullptr;
}

UTextBlock* AWinyunqDemoGameMode::FindTextBlock(FName WidgetName) const
{
    return ActiveAIWerewolfWidget
        ? Cast<UTextBlock>(ActiveAIWerewolfWidget->GetWidgetFromName(WidgetName))
        : nullptr;
}

void AWinyunqDemoGameMode::SetTextBlock(FName WidgetName, const FString& Text)
{
    if (UTextBlock* TextBlock = FindTextBlock(WidgetName))
    {
        TextBlock->SetText(FText::FromString(Text));
    }
}

void AWinyunqDemoGameMode::SetModelStatus(const FString& Text)
{
    SetTextBlock(TEXT("ModelStatusText"), Text);
}

void AWinyunqDemoGameMode::SetDownloadProgress(const FString& Text)
{
    SetTextBlock(TEXT("DownloadProgressText"), Text);
}

void AWinyunqDemoGameMode::SetGameLog(const FString& Text)
{
    SetTextBlock(TEXT("GameLogText"), Text);
}

void AWinyunqDemoGameMode::SetSelectedPlayerCount(int32 Count)
{
    SelectedPlayerCount = Count;

    SetTypedPropertyValue<int32>(ActiveAIWerewolfWidget, TEXT("SelectedPlayerCount"), Count);
    SetTextBlock(TEXT("PlayerCountLabel"), FString::Printf(TEXT("Players / 玩家人数: %d"), Count));
    SetGameLog(FString::Printf(TEXT("Selected player count / 已选择玩家人数: %d."), Count));
}

void AWinyunqDemoGameMode::HandleDownloadE2BClicked()
{
    if (ActiveModelDownload)
    {
        SetModelStatus(TEXT("Model download / 模型下载: already running."));
        return;
    }

    SetWidgetModelReady(false);
    SetModelStatus(TEXT("Model download / 模型下载: starting Gemma 4 E2B. Keep this app open."));
    SetDownloadProgress(TEXT("Download progress / 下载进度: connecting..."));
    SetGameLog(TEXT("Download Gemma 4 E2B LiteRT-LM Model button clicked / 已点击下载模型按钮."));

    ActiveModelDownload = ULiteRtLmDownloadModelAsyncAction::DownloadGemma4E2BModel(this, TEXT(""), false, 7200.0f);
    if (!ActiveModelDownload)
    {
        SetModelStatus(TEXT("Model download / 模型下载: failed to create download task."));
        return;
    }

    ActiveModelDownload->OnProgress.AddDynamic(this, &AWinyunqDemoGameMode::HandleModelDownloadProgress);
    ActiveModelDownload->OnCompleted.AddDynamic(this, &AWinyunqDemoGameMode::HandleModelDownloadCompleted);
    ActiveModelDownload->Activate();
}

void AWinyunqDemoGameMode::HandleLoadDownloadedClicked()
{
    SetModelStatus(TEXT("Model load / 模型加载: checking downloaded file..."));
    SetGameLog(TEXT("Load downloaded model button clicked / 已点击加载已下载模型按钮."));

    if (GetWorld())
    {
        GetWorldTimerManager().SetTimer(
            DeferredLoadModelTimerHandle,
            this,
            &AWinyunqDemoGameMode::LoadDownloadedModelDeferred,
            0.1f,
            false);
    }
}

void AWinyunqDemoGameMode::LoadDownloadedModelDeferred()
{
    const FString DownloadedModelPath = ULiteRtLmBlueprintLibrary::ResolveLiteRtLmDownloadedModelPath(Gemma4E2BModelFileName);
    if (!ULiteRtLmBlueprintLibrary::DoesLiteRtLmDownloadedModelExist(Gemma4E2BModelFileName))
    {
        SetWidgetModelReady(false);
        SetModelStatus(FString::Printf(
            TEXT("Model load / 模型加载: file not found. Use Download first. Path: %s"),
            *DownloadedModelPath));
        return;
    }

    SetModelStatus(TEXT("Model load / 模型加载: loading with GPU backend..."));
    bool bLoaded = ULiteRtLmBlueprintLibrary::LoadLiteRtLmDownloadedModel(
        Gemma4E2BModelFileName,
        true,
        TEXT("gpu"),
        2048,
        8,
        false,
        true,
        false,
        false,
        true);

    if (!bLoaded)
    {
        SetModelStatus(TEXT("Model load / 模型加载: GPU load failed, retrying CPU backend..."));
        bLoaded = ULiteRtLmBlueprintLibrary::LoadLiteRtLmDownloadedModel(
            Gemma4E2BModelFileName,
            true,
            TEXT("cpu"),
            2048,
            4,
            false,
            true,
            false,
            false,
            true);
    }

    SetWidgetModelReady(bLoaded);
    SetModelStatus(bLoaded
        ? TEXT("Model load / 模型加载: ready. You can start the game.")
        : TEXT("Model load / 模型加载: failed. Check Android logcat for LiteRT-LM details."));
    SetGameLog(bLoaded
        ? TEXT("Gemma model loaded / Gemma 模型已加载.")
        : TEXT("Gemma model load failed / Gemma 模型加载失败."));
}

void AWinyunqDemoGameMode::HandleImportModelClicked()
{
    const FString DownloadedModelPath = ULiteRtLmBlueprintLibrary::ResolveLiteRtLmDownloadedModelPath(Gemma4E2BModelFileName);
    SetModelStatus(FString::Printf(
        TEXT("Import model / 导入模型: Android file picker is not wired in this build. Use Download, or place the model at: %s"),
        *DownloadedModelPath));
    SetGameLog(TEXT("Import button clicked / 已点击导入按钮. This build uses the downloaded-model path."));
}

void AWinyunqDemoGameMode::HandleStartGameClicked()
{
    ExecuteDirectorFunction(TEXT("StartNewGame"), TEXT("Start New Game"));
}

void AWinyunqDemoGameMode::HandleAutoTestClicked()
{
    ExecuteDirectorFunction(TEXT("RunAutomatedAITest"), TEXT("Run Automated AI Test"));
}

void AWinyunqDemoGameMode::HandleResetGameClicked()
{
    ExecuteDirectorFunction(TEXT("StartNewGame"), TEXT("Reset Game"));
}

void AWinyunqDemoGameMode::HandlePlayer5Clicked()
{
    SetSelectedPlayerCount(5);
}

void AWinyunqDemoGameMode::HandlePlayer6Clicked()
{
    SetSelectedPlayerCount(6);
}

void AWinyunqDemoGameMode::HandlePlayer7Clicked()
{
    SetSelectedPlayerCount(7);
}

void AWinyunqDemoGameMode::HandlePlayer8Clicked()
{
    SetSelectedPlayerCount(8);
}

void AWinyunqDemoGameMode::HandlePlayer9Clicked()
{
    SetSelectedPlayerCount(9);
}

void AWinyunqDemoGameMode::HandlePlayer10Clicked()
{
    SetSelectedPlayerCount(10);
}

void AWinyunqDemoGameMode::HandleModelDownloadProgress(int64 BytesReceived, int64 ContentLength, float Progress)
{
    const double ReceivedMB = static_cast<double>(BytesReceived) / BytesPerMegabyte;
    if (ContentLength > 0)
    {
        const double TotalMB = static_cast<double>(ContentLength) / BytesPerMegabyte;
        SetDownloadProgress(FString::Printf(
            TEXT("Download progress / 下载进度: %.1f%% (%.1f / %.1f MB)"),
            Progress * 100.0f,
            ReceivedMB,
            TotalMB));
    }
    else
    {
        SetDownloadProgress(FString::Printf(
            TEXT("Download progress / 下载进度: %.1f MB received."),
            ReceivedMB));
    }
}

void AWinyunqDemoGameMode::HandleModelDownloadCompleted(const FLiteRtLmModelDownloadResult& Result)
{
    ActiveModelDownload = nullptr;
    SetWidgetModelReady(Result.bSuccess);

    if (Result.bSuccess)
    {
        SetDownloadProgress(FString::Printf(
            TEXT("Download progress / 下载进度: complete. %.1f MB saved."),
            static_cast<double>(Result.BytesReceived) / BytesPerMegabyte));
        SetModelStatus(FString::Printf(
            TEXT("Model download / 模型下载: ready%s. Path: %s"),
            Result.bAlreadyExists ? TEXT(" (already existed)") : TEXT(""),
            *Result.LocalPath));
        SetGameLog(TEXT("Model is ready. Tap Load Downloaded Model before starting the game."));
    }
    else
    {
        SetDownloadProgress(TEXT("Download progress / 下载进度: failed."));
        SetModelStatus(FString::Printf(
            TEXT("Model download / 模型下载: failed. HTTP %d. %s"),
            Result.HttpStatus,
            *Result.ErrorMessage));
        SetGameLog(TEXT("Download failed. Check network permission, storage, and the model URL."));
    }
}

AActor* AWinyunqDemoGameMode::GetOrCreateAIWerewolfDirector()
{
    if (IsValid(ActiveAIWerewolfDirector))
    {
        return ActiveAIWerewolfDirector;
    }

    if (!AIWerewolfDirectorClass)
    {
        return nullptr;
    }

    if (AActor* ExistingDirector = UGameplayStatics::GetActorOfClass(this, AIWerewolfDirectorClass))
    {
        ActiveAIWerewolfDirector = ExistingDirector;
        return ActiveAIWerewolfDirector;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ActiveAIWerewolfDirector = World->SpawnActor<AActor>(
        AIWerewolfDirectorClass,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        SpawnParameters);
    return ActiveAIWerewolfDirector;
}

void AWinyunqDemoGameMode::ExecuteDirectorFunction(FName FunctionName, const FString& ActionLabel)
{
    AActor* Director = GetOrCreateAIWerewolfDirector();
    if (!Director)
    {
        SetGameLog(FString::Printf(
            TEXT("%s clicked, but BP_AIWerewolfDirector was not found or spawned."),
            *ActionLabel));
        return;
    }

    ApplySelectedPlayerCountToObject(Director);

    if (UFunction* Function = Director->FindFunction(FunctionName))
    {
        Director->ProcessEvent(Function, nullptr);
        SetGameLog(FString::Printf(
            TEXT("%s sent to BP_AIWerewolfDirector. Selected players: %d."),
            *ActionLabel,
            SelectedPlayerCount));
        return;
    }

    SetGameLog(FString::Printf(
        TEXT("%s clicked, but function %s was not found on BP_AIWerewolfDirector."),
        *ActionLabel,
        *FunctionName.ToString()));
}

void AWinyunqDemoGameMode::ApplySelectedPlayerCountToObject(UObject* Target) const
{
    if (!Target)
    {
        return;
    }

    SetTypedPropertyValue<int32>(Target, TEXT("SelectedPlayerCount"), SelectedPlayerCount);
    SetTypedPropertyValue<int32>(Target, TEXT("PlayerCount"), SelectedPlayerCount);
}

void AWinyunqDemoGameMode::SetWidgetModelReady(bool bReady) const
{
    SetTypedPropertyValue<bool>(ActiveAIWerewolfWidget, TEXT("bModelReady"), bReady);
}
