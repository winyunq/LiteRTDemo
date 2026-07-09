#include "WinyunqDemoGameMode.h"
#include "DemoTavernHUD.h"
#include "Async/Async.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "LiteRtLmBlueprintLibrary.h"
#include "LiteRtLmModelDownloader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "WinyunqMcpWrapper.h"
#include "Blueprint/UserWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <commdlg.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#if PLATFORM_ANDROID
#include "Android/AndroidJNI.h"
#include "Android/AndroidJavaEnv.h"
#endif

namespace
{
constexpr TCHAR Gemma4E2BModelFileName[] = TEXT("gemma-4-E2B-it.litertlm");
constexpr double BytesPerMegabyte = 1024.0 * 1024.0;
constexpr int32 MaxRuntimePlayers = 10;
constexpr double RuntimeAIWatchdogIntervalSeconds = 5.0;
constexpr double RuntimeAIStallWarningSeconds = 20.0;
constexpr double RuntimeAIRequestTimeoutSeconds = 90.0;

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

template <typename WidgetType>
WidgetType* ConstructRuntimeWidget(UUserWidget* OwnerWidget, FName WidgetName)
{
    if (!OwnerWidget || !OwnerWidget->WidgetTree)
    {
        return nullptr;
    }

    return OwnerWidget->WidgetTree->ConstructWidget<WidgetType>(WidgetType::StaticClass(), WidgetName);
}

FLinearColor GetRuntimeAvatarColor(int32 PlayerIndex)
{
    static const FLinearColor Colors[] =
    {
        FLinearColor(0.22f, 0.48f, 0.95f, 1.0f),
        FLinearColor(0.90f, 0.24f, 0.24f, 1.0f),
        FLinearColor(0.18f, 0.70f, 0.42f, 1.0f),
        FLinearColor(0.92f, 0.64f, 0.18f, 1.0f),
        FLinearColor(0.62f, 0.34f, 0.88f, 1.0f),
        FLinearColor(0.18f, 0.68f, 0.78f, 1.0f),
        FLinearColor(0.86f, 0.36f, 0.62f, 1.0f),
        FLinearColor(0.50f, 0.58f, 0.24f, 1.0f),
        FLinearColor(0.70f, 0.42f, 0.20f, 1.0f),
        FLinearColor(0.38f, 0.42f, 0.52f, 1.0f)
    };

    return Colors[FMath::Clamp(PlayerIndex, 0, UE_ARRAY_COUNT(Colors) - 1)];
}

#if PLATFORM_ANDROID
constexpr int32 AIWerewolfImportModelRequestCode = 7816;

bool CopyAndroidUriToFile(JNIEnv* Env, jobject IntentData, const FString& TargetPath, FString& OutErrorMessage)
{
    if (!Env || !IntentData)
    {
        OutErrorMessage = TEXT("Android document picker did not return a file.");
        return false;
    }

    jclass IntentClass = Env->GetObjectClass(IntentData);
    jmethodID GetDataMethod = Env->GetMethodID(IntentClass, "getData", "()Landroid/net/Uri;");
    jobject UriObject = Env->CallObjectMethod(IntentData, GetDataMethod);
    if (AndroidJavaEnv::CheckJavaException() || !UriObject)
    {
        OutErrorMessage = TEXT("Could not read selected Android document URI.");
        return false;
    }

    jobject ActivityObject = FJavaWrapper::GameActivityThis;
    jclass ActivityClass = Env->GetObjectClass(ActivityObject);
    jmethodID GetContentResolverMethod = Env->GetMethodID(ActivityClass, "getContentResolver", "()Landroid/content/ContentResolver;");
    jobject ResolverObject = Env->CallObjectMethod(ActivityObject, GetContentResolverMethod);
    if (AndroidJavaEnv::CheckJavaException() || !ResolverObject)
    {
        OutErrorMessage = TEXT("Could not access Android ContentResolver.");
        return false;
    }

    jclass ResolverClass = Env->GetObjectClass(ResolverObject);
    jmethodID OpenInputStreamMethod = Env->GetMethodID(ResolverClass, "openInputStream", "(Landroid/net/Uri;)Ljava/io/InputStream;");
    jobject InputStreamObject = Env->CallObjectMethod(ResolverObject, OpenInputStreamMethod, UriObject);
    if (AndroidJavaEnv::CheckJavaException() || !InputStreamObject)
    {
        OutErrorMessage = TEXT("Could not open the selected model file for reading.");
        return false;
    }

    IFileManager::Get().MakeDirectory(*FPaths::GetPath(TargetPath), true);
    const FString PartialPath = TargetPath + TEXT(".part");
    IFileManager::Get().Delete(*PartialPath, false, true, true);

    TUniquePtr<FArchive> Writer(IFileManager::Get().CreateFileWriter(*PartialPath));
    if (!Writer.IsValid())
    {
        OutErrorMessage = FString::Printf(TEXT("Could not create target model file: %s"), *PartialPath);
        return false;
    }

    jclass InputStreamClass = Env->GetObjectClass(InputStreamObject);
    jmethodID ReadMethod = Env->GetMethodID(InputStreamClass, "read", "([B)I");
    jmethodID CloseMethod = Env->GetMethodID(InputStreamClass, "close", "()V");

    constexpr int32 BufferSize = 256 * 1024;
    jbyteArray JavaBuffer = Env->NewByteArray(BufferSize);
    TArray<int8> NativeBuffer;
    NativeBuffer.SetNumUninitialized(BufferSize);

    bool bCopySucceeded = true;
    while (true)
    {
        const jint BytesRead = Env->CallIntMethod(InputStreamObject, ReadMethod, JavaBuffer);
        if (AndroidJavaEnv::CheckJavaException())
        {
            OutErrorMessage = TEXT("Read failed while importing Android model file.");
            bCopySucceeded = false;
            break;
        }

        if (BytesRead < 0)
        {
            break;
        }

        if (BytesRead == 0)
        {
            continue;
        }

        Env->GetByteArrayRegion(JavaBuffer, 0, BytesRead, NativeBuffer.GetData());
        Writer->Serialize(NativeBuffer.GetData(), BytesRead);
        if (Writer->IsError())
        {
            OutErrorMessage = FString::Printf(TEXT("Write failed while importing model to: %s"), *PartialPath);
            bCopySucceeded = false;
            break;
        }
    }

    Env->CallVoidMethod(InputStreamObject, CloseMethod);
    AndroidJavaEnv::CheckJavaException();
    Writer.Reset();

    Env->DeleteLocalRef(JavaBuffer);
    Env->DeleteLocalRef(InputStreamObject);
    Env->DeleteLocalRef(ResolverObject);
    Env->DeleteLocalRef(UriObject);

    if (!bCopySucceeded)
    {
        IFileManager::Get().Delete(*PartialPath, false, true, true);
        return false;
    }

    if (!IFileManager::Get().Move(*TargetPath, *PartialPath, true, true, false, true))
    {
        IFileManager::Get().Delete(*PartialPath, false, true, true);
        OutErrorMessage = FString::Printf(TEXT("Could not move imported model to: %s"), *TargetPath);
        return false;
    }

    return true;
}
#endif
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
            ApplyRuntimeCommandLineOptions();

            const bool bAutoStart = FParse::Param(FCommandLine::Get(), TEXT("AIWerewolfAutoStart"));
            const bool bAutoLoadModel = FParse::Param(FCommandLine::Get(), TEXT("AIWerewolfAutoLoadModel"));
            bStartGameAfterModelLoad = bAutoStart && bAutoLoadModel;

            if (bAutoLoadModel)
            {
                GetWorldTimerManager().SetTimer(
                    DeferredLoadModelTimerHandle,
                    this,
                    &AWinyunqDemoGameMode::LoadDownloadedModelDeferred,
                    0.2f,
                    false);
            }
            else if (bAutoStart)
            {
                GetWorldTimerManager().SetTimerForNextTick(this, &AWinyunqDemoGameMode::StartRuntimeWerewolfGame);
            }
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
            Button->SetTouchMethod(EButtonTouchMethod::PreciseTap);
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
    SetTextBlock(TEXT("DownloadE2BText"), TEXT("Download Gemma 4 E2B / 下载模型"));
    SetTextBlock(TEXT("LoadDownloadedText"), TEXT("Load Downloaded Model / 加载已下载模型"));
    SetTextBlock(TEXT("ImportModelText"), TEXT("Import Model / 导入模型"));
    BuildSetupLanguageControl();
    RefreshRuntimeLanguageLabels();

    const FString MissingSummary = MissingButtons.Num() > 0
        ? FString::Printf(TEXT(" Missing: %s."), *FString::Join(MissingButtons, TEXT(", ")))
        : FString();

    SetModelStatus(TEXT("Model status / 模型状态: use Download, or Import Model to copy an existing .litertlm file."));
    SetDownloadProgress(TEXT("Download progress / 下载进度: idle."));
    SetGameLog(FString::Printf(
        TEXT("Setup controls bound at runtime / 设置按钮已在运行时绑定: %d buttons.%s"),
        BoundButtonCount,
        *MissingSummary));
}

void AWinyunqDemoGameMode::ApplyRuntimeCommandLineOptions()
{
    FString LanguageArg;
    if (FParse::Value(FCommandLine::Get(), TEXT("AIWerewolfLanguage="), LanguageArg))
    {
        LanguageArg = LanguageArg.ToLower();
        if (LanguageArg == TEXT("zh") || LanguageArg == TEXT("cn") || LanguageArg == TEXT("chinese"))
        {
            SetRuntimeLanguage(EAIWerewolfRuntimeLanguage::Chinese);
        }
        else if (LanguageArg == TEXT("en") || LanguageArg == TEXT("english"))
        {
            SetRuntimeLanguage(EAIWerewolfRuntimeLanguage::English);
        }
        else
        {
            SetRuntimeLanguage(EAIWerewolfRuntimeLanguage::Bilingual);
        }
    }

    bRuntimeAutoPlayHuman = FParse::Param(FCommandLine::Get(), TEXT("AIWerewolfAutoPlayHuman"));
    FParse::Value(FCommandLine::Get(), TEXT("AIWerewolfAutoPlayMaxRounds="), RuntimeAutoPlayMaxRounds);
    RuntimeAutoPlayMaxRounds = FMath::Clamp(RuntimeAutoPlayMaxRounds, 1, 12);

    if (bRuntimeAutoPlayHuman)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AIWerewolf] Human autoplay enabled. MaxRounds=%d Language=%s"),
            RuntimeAutoPlayMaxRounds,
            *GetRuntimeLanguageLabel());
        SetGameLog(FString::Printf(TEXT("Auto play human enabled / 自动玩家已启用. Language=%s, MaxRounds=%d."),
            *GetRuntimeLanguageLabel(),
            RuntimeAutoPlayMaxRounds));
    }
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
    SetTextBlock(TEXT("PlayerCountLabel"), FString::Printf(TEXT("%s: %d"), *LocalizeRuntimeText(TEXT("Players"), TEXT("玩家人数")), Count));
    SetGameLog(FString::Printf(TEXT("%s: %d."), *LocalizeRuntimeText(TEXT("Selected player count"), TEXT("已选择玩家人数")), Count));
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
    if (bModelLoadInFlight)
    {
        SetModelStatus(TEXT("Model load / 模型加载: already loading..."));
        return;
    }

    const FString DownloadedModelPath = ULiteRtLmBlueprintLibrary::ResolveLiteRtLmDownloadedModelPath(Gemma4E2BModelFileName);
    FString ModelPath = DownloadedModelPath;
    if (!ULiteRtLmBlueprintLibrary::DoesLiteRtLmDownloadedModelExist(Gemma4E2BModelFileName))
    {
        const FString ProjectModelPath = ULiteRtLmBlueprintLibrary::ResolveLiteRtLmProjectModelPath(Gemma4E2BModelFileName);
        if (!ProjectModelPath.IsEmpty() && IFileManager::Get().FileExists(*ProjectModelPath))
        {
            ModelPath = ProjectModelPath;
        }
        else
        {
            SetWidgetModelReady(false);
            SetModelStatus(FString::Printf(
                TEXT("Model load / 模型加载: file not found. Use Download or Import first. Path: %s"),
                *DownloadedModelPath));
            return;
        }
    }

    const FLiteRtLmConfig InitialConfig = BuildRuntimeModelConfig(ModelPath);
    SetWidgetModelReady(false);
    bModelLoadInFlight = true;
    SetModelStatus(FString::Printf(
        TEXT("Model load / 模型加载: loading asynchronously with %s backend..."),
        *InitialConfig.Backend));
    SetGameLog(FString::Printf(TEXT("Loading LiteRT-LM model from: %s"), *ModelPath));

    TWeakObjectPtr<AWinyunqDemoGameMode> WeakThis(this);
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [WeakThis, InitialConfig]()
    {
        FLiteRtLmConfig Config = InitialConfig;
        FString UsedBackend = Config.Backend;
        bool bLoaded = ULiteRtLmBlueprintLibrary::LoadLiteRtLmModel(Config);

#if !PLATFORM_ANDROID
        if (!bLoaded && !Config.Backend.Equals(TEXT("cpu"), ESearchCase::IgnoreCase))
        {
            Config.Backend = TEXT("cpu");
            Config.NumThreads = 4;
            Config.MaxNumTokens = FMath::Min(Config.MaxNumTokens, 1024);
            Config.PrefillChunkSize = FMath::Min(Config.PrefillChunkSize, 512);
            Config.bOptimizeShader = false;
            UsedBackend = Config.Backend;
            bLoaded = ULiteRtLmBlueprintLibrary::LoadLiteRtLmModel(Config);
        }
#endif

        AsyncTask(ENamedThreads::GameThread, [WeakThis, bLoaded, UsedBackend]()
        {
            if (!WeakThis.IsValid())
            {
                return;
            }

            AWinyunqDemoGameMode* StrongThis = WeakThis.Get();
            StrongThis->bModelLoadInFlight = false;
            StrongThis->SetWidgetModelReady(bLoaded);
            StrongThis->SetModelStatus(bLoaded
                ? FString::Printf(TEXT("Model load / 模型加载: ready on %s backend. You can start the game."), *UsedBackend)
                : TEXT("Model load / 模型加载: failed. Check Android logcat for LiteRT-LM details."));
            StrongThis->SetGameLog(bLoaded
                ? TEXT("Gemma model loaded / Gemma 模型已加载.")
                : TEXT("Gemma model load failed / Gemma 模型加载失败."));
            if (bLoaded && StrongThis->bStartGameAfterModelLoad)
            {
                StrongThis->bStartGameAfterModelLoad = false;
                StrongThis->StartRuntimeWerewolfGame();
            }
        });
    });
}

FString AWinyunqDemoGameMode::GetRuntimeImportTargetPath() const
{
    return ULiteRtLmBlueprintLibrary::ResolveLiteRtLmDownloadedModelPath(Gemma4E2BModelFileName);
}

bool AWinyunqDemoGameMode::CopyModelFileStreaming(const FString& SourcePath, const FString& TargetPath, FString& OutErrorMessage) const
{
    FString NormalizedSource = FPaths::ConvertRelativePathToFull(SourcePath);
    FString NormalizedTarget = FPaths::ConvertRelativePathToFull(TargetPath);
    FPaths::NormalizeFilename(NormalizedSource);
    FPaths::NormalizeFilename(NormalizedTarget);

    if (NormalizedSource.Equals(NormalizedTarget, ESearchCase::IgnoreCase))
    {
        return IFileManager::Get().FileExists(*NormalizedTarget);
    }

    if (!IFileManager::Get().FileExists(*NormalizedSource))
    {
        OutErrorMessage = FString::Printf(TEXT("Source model file does not exist: %s"), *NormalizedSource);
        return false;
    }

    TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*NormalizedSource));
    if (!Reader.IsValid())
    {
        OutErrorMessage = FString::Printf(TEXT("Could not open model file for read: %s"), *NormalizedSource);
        return false;
    }

    IFileManager::Get().MakeDirectory(*FPaths::GetPath(NormalizedTarget), true);
    const FString PartialPath = NormalizedTarget + TEXT(".part");
    IFileManager::Get().Delete(*PartialPath, false, true, true);

    TUniquePtr<FArchive> Writer(IFileManager::Get().CreateFileWriter(*PartialPath));
    if (!Writer.IsValid())
    {
        OutErrorMessage = FString::Printf(TEXT("Could not create imported model file: %s"), *PartialPath);
        return false;
    }

    TArray<uint8> Buffer;
    Buffer.SetNumUninitialized(1024 * 1024);

    int64 RemainingBytes = Reader->TotalSize();
    if (RemainingBytes <= 0)
    {
        OutErrorMessage = FString::Printf(TEXT("Selected model file is empty: %s"), *NormalizedSource);
        Writer.Reset();
        Reader.Reset();
        IFileManager::Get().Delete(*PartialPath, false, true, true);
        return false;
    }

    while (RemainingBytes > 0)
    {
        const int64 ChunkSize64 = FMath::Min<int64>(RemainingBytes, Buffer.Num());
        const int32 ChunkSize = static_cast<int32>(ChunkSize64);
        Reader->Serialize(Buffer.GetData(), ChunkSize);
        if (Reader->IsError())
        {
            OutErrorMessage = FString::Printf(TEXT("Read failed while importing model: %s"), *NormalizedSource);
            Writer.Reset();
            Reader.Reset();
            IFileManager::Get().Delete(*PartialPath, false, true, true);
            return false;
        }

        Writer->Serialize(Buffer.GetData(), ChunkSize);
        if (Writer->IsError())
        {
            OutErrorMessage = FString::Printf(TEXT("Write failed while importing model: %s"), *PartialPath);
            Writer.Reset();
            Reader.Reset();
            IFileManager::Get().Delete(*PartialPath, false, true, true);
            return false;
        }

        RemainingBytes -= ChunkSize64;
    }

    Writer.Reset();
    Reader.Reset();

    if (!IFileManager::Get().Move(*NormalizedTarget, *PartialPath, true, true, false, true))
    {
        IFileManager::Get().Delete(*PartialPath, false, true, true);
        OutErrorMessage = FString::Printf(TEXT("Could not move imported model to: %s"), *NormalizedTarget);
        return false;
    }

    return true;
}

bool AWinyunqDemoGameMode::ImportModelFromPath(const FString& SourcePath, FString& OutImportedPath, FString& OutErrorMessage)
{
    OutImportedPath = GetRuntimeImportTargetPath();
    OutErrorMessage.Reset();

    if (SourcePath.IsEmpty())
    {
        OutErrorMessage = TEXT("No model file was selected.");
        return false;
    }

    if (!FPaths::GetCleanFilename(SourcePath).EndsWith(TEXT(".litertlm"), ESearchCase::IgnoreCase))
    {
        OutErrorMessage = FString::Printf(TEXT("Selected file is not a .litertlm model: %s"), *SourcePath);
        return false;
    }

    return CopyModelFileStreaming(SourcePath, OutImportedPath, OutErrorMessage);
}

void AWinyunqDemoGameMode::FinishModelImport(bool bSuccess, const FString& ImportedPath, const FString& ErrorMessage)
{
    if (bSuccess)
    {
        SetWidgetModelReady(false);
        SetModelStatus(FString::Printf(TEXT("Import model / 导入模型: complete. Saved to: %s"), *ImportedPath));
        SetDownloadProgress(TEXT("Import complete / 导入完成."));
        SetGameLog(TEXT("Imported model copied to LiteRT-LM downloaded-model storage."));

#if PLATFORM_ANDROID
        SetModelStatus(FString::Printf(TEXT("Import model / 导入模型: complete. Tap Load Downloaded Model to load it. Path: %s"), *ImportedPath));
        SetGameLog(TEXT("Import complete. Tap Load Downloaded Model when you are ready to load the 2.6GB model."));
#else
        if (GetWorld())
        {
            GetWorldTimerManager().SetTimer(
                DeferredLoadModelTimerHandle,
                this,
                &AWinyunqDemoGameMode::LoadDownloadedModelDeferred,
                0.1f,
                false);
        }
#endif
        return;
    }

    SetWidgetModelReady(false);
    SetModelStatus(FString::Printf(TEXT("Import model / 导入模型: failed. %s"), *ErrorMessage));
    SetDownloadProgress(TEXT("Import failed / 导入失败."));
    SetGameLog(TEXT("Import failed. On Android, use the system document picker and select gemma-4-E2B-it.litertlm."));
}

void AWinyunqDemoGameMode::BeginModelImport()
{
    SetWidgetModelReady(false);
    SetModelStatus(TEXT("Import model / 导入模型: choose an existing .litertlm model file."));
    SetDownloadProgress(TEXT("Import progress / 导入进度: waiting for file selection..."));
    SetGameLog(TEXT("Import button clicked. The model will be copied to LiteRT-LM downloaded-model storage."));

    FString ImportedPath;
    FString ErrorMessage;

#if PLATFORM_WINDOWS
    if (ImportModelWithWindowsFilePicker(ImportedPath, ErrorMessage))
    {
        FinishModelImport(true, ImportedPath, TEXT(""));
    }
    else
    {
        FinishModelImport(false, ImportedPath, ErrorMessage);
    }
#elif PLATFORM_ANDROID
    if (BeginAndroidSafModelImport(ErrorMessage))
    {
        SetModelStatus(TEXT("Import model / 导入模型: Android document picker opened. Select gemma-4-E2B-it.litertlm."));
        return;
    }

    if (TryImportModelFromAndroidCommonPaths(ImportedPath, ErrorMessage))
    {
        FinishModelImport(true, ImportedPath, TEXT(""));
        return;
    }

    FinishModelImport(false, ImportedPath, ErrorMessage);
#else
    FinishModelImport(false, ImportedPath, TEXT("Model import is currently implemented for Windows and Android only."));
#endif
}

#if PLATFORM_WINDOWS
bool AWinyunqDemoGameMode::ImportModelWithWindowsFilePicker(FString& OutImportedPath, FString& OutErrorMessage)
{
    TCHAR FileNameBuffer[32768] = {};

    OPENFILENAME OpenFileName;
    FMemory::Memzero(OpenFileName);
    OpenFileName.lStructSize = sizeof(OPENFILENAME);
    OpenFileName.hwndOwner = nullptr;
    OpenFileName.lpstrFilter = TEXT("LiteRT-LM Model (*.litertlm)\0*.litertlm\0All Files (*.*)\0*.*\0");
    OpenFileName.lpstrFile = FileNameBuffer;
    OpenFileName.nMaxFile = UE_ARRAY_COUNT(FileNameBuffer);
    OpenFileName.lpstrTitle = TEXT("Select Gemma 4 E2B LiteRT-LM model");
    OpenFileName.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetOpenFileName(&OpenFileName))
    {
        const DWORD DialogError = CommDlgExtendedError();
        OutErrorMessage = DialogError == 0
            ? TEXT("Import cancelled.")
            : FString::Printf(TEXT("Windows file picker failed. Error code: %lu"), static_cast<unsigned long>(DialogError));
        return false;
    }

    return ImportModelFromPath(FileNameBuffer, OutImportedPath, OutErrorMessage);
}
#endif

#if PLATFORM_ANDROID
bool AWinyunqDemoGameMode::BeginAndroidSafModelImport(FString& OutErrorMessage)
{
#if USE_ANDROID_JNI
    JNIEnv* Env = AndroidJavaEnv::GetJavaEnv();
    if (!Env || !FJavaWrapper::GameActivityThis)
    {
        OutErrorMessage = TEXT("Android JNI environment is not ready.");
        return false;
    }

    if (AndroidImportActivityResultHandle.IsValid())
    {
        FJavaWrapper::OnActivityResultDelegate.Remove(AndroidImportActivityResultHandle);
        AndroidImportActivityResultHandle.Reset();
    }

    TWeakObjectPtr<AWinyunqDemoGameMode> WeakThis(this);
    AndroidImportActivityResultHandle = FJavaWrapper::OnActivityResultDelegate.AddLambda(
        [WeakThis](JNIEnv* ResultEnv, jobject, jobject, jint RequestCode, jint ResultCode, jobject IntentData)
        {
            if (RequestCode != AIWerewolfImportModelRequestCode)
            {
                return;
            }

            if (!WeakThis.IsValid())
            {
                return;
            }

            FJavaWrapper::OnActivityResultDelegate.Remove(WeakThis->AndroidImportActivityResultHandle);
            WeakThis->AndroidImportActivityResultHandle.Reset();

            FString ImportedPath = WeakThis->GetRuntimeImportTargetPath();
            FString ErrorMessage;
            const bool bSuccess = ResultCode == -1
                ? CopyAndroidUriToFile(ResultEnv, IntentData, ImportedPath, ErrorMessage)
                : false;

            if (!bSuccess && ErrorMessage.IsEmpty())
            {
                ErrorMessage = TEXT("Android model import cancelled.");
            }

            AsyncTask(ENamedThreads::GameThread, [WeakThis, bSuccess, ImportedPath, ErrorMessage]()
            {
                if (WeakThis.IsValid())
                {
                    WeakThis->FinishModelImport(bSuccess, ImportedPath, ErrorMessage);
                }
            });
        });

    jclass IntentClass = Env->FindClass("android/content/Intent");
    jmethodID IntentConstructor = Env->GetMethodID(IntentClass, "<init>", "(Ljava/lang/String;)V");
    jstring ActionOpenDocument = Env->NewStringUTF("android.intent.action.OPEN_DOCUMENT");
    jobject IntentObject = Env->NewObject(IntentClass, IntentConstructor, ActionOpenDocument);

    jmethodID AddCategoryMethod = Env->GetMethodID(IntentClass, "addCategory", "(Ljava/lang/String;)Landroid/content/Intent;");
    jstring CategoryOpenable = Env->NewStringUTF("android.intent.category.OPENABLE");
    Env->CallObjectMethod(IntentObject, AddCategoryMethod, CategoryOpenable);

    jmethodID SetTypeMethod = Env->GetMethodID(IntentClass, "setType", "(Ljava/lang/String;)Landroid/content/Intent;");
    jstring AnyMime = Env->NewStringUTF("*/*");
    Env->CallObjectMethod(IntentObject, SetTypeMethod, AnyMime);

    jclass ActivityClass = Env->GetObjectClass(FJavaWrapper::GameActivityThis);
    jmethodID StartActivityForResultMethod = Env->GetMethodID(ActivityClass, "startActivityForResult", "(Landroid/content/Intent;I)V");
    Env->CallVoidMethod(FJavaWrapper::GameActivityThis, StartActivityForResultMethod, IntentObject, AIWerewolfImportModelRequestCode);

    const bool bHadException = AndroidJavaEnv::CheckJavaException();

    Env->DeleteLocalRef(AnyMime);
    Env->DeleteLocalRef(CategoryOpenable);
    Env->DeleteLocalRef(ActionOpenDocument);
    Env->DeleteLocalRef(IntentObject);
    Env->DeleteLocalRef(IntentClass);

    if (bHadException)
    {
        FJavaWrapper::OnActivityResultDelegate.Remove(AndroidImportActivityResultHandle);
        AndroidImportActivityResultHandle.Reset();
        OutErrorMessage = TEXT("Android document picker could not be opened.");
        return false;
    }

    return true;
#else
    OutErrorMessage = TEXT("Android JNI is not enabled.");
    return false;
#endif
}

bool AWinyunqDemoGameMode::TryImportModelFromAndroidCommonPaths(FString& OutImportedPath, FString& OutErrorMessage)
{
    TArray<FString> CandidatePaths;
    CandidatePaths.Add(TEXT("/sdcard/Download/gemma-4-E2B-it.litertlm"));
    CandidatePaths.Add(TEXT("/sdcard/Downloads/gemma-4-E2B-it.litertlm"));
    CandidatePaths.Add(TEXT("/storage/emulated/0/Download/gemma-4-E2B-it.litertlm"));
    CandidatePaths.Add(TEXT("/storage/emulated/0/Downloads/gemma-4-E2B-it.litertlm"));

    for (const FString& CandidatePath : CandidatePaths)
    {
        if (IFileManager::Get().FileExists(*CandidatePath))
        {
            if (ImportModelFromPath(CandidatePath, OutImportedPath, OutErrorMessage))
            {
                return true;
            }
        }
    }

    OutErrorMessage = TEXT("Android document picker failed and no model was found in /sdcard/Download/gemma-4-E2B-it.litertlm.");
    return false;
}
#endif

void AWinyunqDemoGameMode::HandleImportModelClicked()
{
    BeginModelImport();
}

void AWinyunqDemoGameMode::HandleStartGameClicked()
{
    StartRuntimeWerewolfGame();
}

void AWinyunqDemoGameMode::HandleAutoTestClicked()
{
    bRuntimeAutoPlayHuman = true;
    RuntimeAutoPlayMaxRounds = FMath::Max(RuntimeAutoPlayMaxRounds, 2);
    if (RuntimePhase == EAIWerewolfRuntimePhase::Setup)
    {
        StartRuntimeWerewolfGame();
    }

    if (RuntimePhase == EAIWerewolfRuntimePhase::Discussion)
    {
        EnterRuntimeVotingPhase();
    }
    StartRuntimeAutoPlayTimerIfNeeded();
    SetGameLog(LocalizeRuntimeText(TEXT("Auto test enabled."), TEXT("自动测试已启用。")));
}

void AWinyunqDemoGameMode::HandleLanguageToggleClicked()
{
    ToggleRuntimeLanguage();
}

void AWinyunqDemoGameMode::HandleResetGameClicked()
{
    ResetRuntimeWerewolfGame();
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

void AWinyunqDemoGameMode::BuildSetupLanguageControl()
{
    if (SetupLanguageButtonText || !ActiveAIWerewolfWidget)
    {
        return;
    }

    if (UButton* ExistingLanguageButton = FindButton(TEXT("LanguageButton")))
    {
        BindRuntimeButton(ExistingLanguageButton, GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleLanguageToggleClicked));
        SetupLanguageButtonText = FindTextBlock(TEXT("LanguageText"));
        return;
    }

    UPanelWidget* RootPanel = Cast<UPanelWidget>(ActiveAIWerewolfWidget->GetWidgetFromName(TEXT("RootCanvas")));
    if (!RootPanel && ActiveAIWerewolfWidget->WidgetTree)
    {
        RootPanel = Cast<UPanelWidget>(ActiveAIWerewolfWidget->WidgetTree->RootWidget);
    }

    if (!RootPanel)
    {
        return;
    }

    UTextBlock* LanguageLabel = nullptr;
    UButton* LanguageButton = CreateRuntimeButton(
        TEXT("RuntimeSetupLanguageButton"),
        TEXT("Language / 语言"),
        GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleLanguageToggleClicked),
        FLinearColor(0.18f, 0.24f, 0.34f, 1.0f),
        &LanguageLabel);
    if (!LanguageButton)
    {
        return;
    }

    SetupLanguageButtonText = LanguageLabel;
    RootPanel->AddChild(LanguageButton);
    if (UCanvasPanelSlot* LanguageSlot = Cast<UCanvasPanelSlot>(LanguageButton->Slot))
    {
        LanguageSlot->SetAnchors(FAnchors(1.0f, 0.0f));
        LanguageSlot->SetAlignment(FVector2D(1.0f, 0.0f));
        LanguageSlot->SetPosition(FVector2D(-18.0f, 18.0f));
        LanguageSlot->SetSize(FVector2D(230.0f, 46.0f));
    }
}

void AWinyunqDemoGameMode::SetRuntimeLanguage(EAIWerewolfRuntimeLanguage NewLanguage)
{
    RuntimeLanguage = NewLanguage;
    RefreshRuntimeLanguageLabels();
    RefreshRuntimePhaseText();
    RefreshRuntimePlayers();
    if (RuntimePlayerInput)
    {
        RuntimePlayerInput->SetHintText(FText::FromString(LocalizeRuntimeText(TEXT("Type your speech"), TEXT("输入你的发言"))));
    }
}

void AWinyunqDemoGameMode::ToggleRuntimeLanguage()
{
    switch (RuntimeLanguage)
    {
    case EAIWerewolfRuntimeLanguage::English:
        SetRuntimeLanguage(EAIWerewolfRuntimeLanguage::Chinese);
        break;
    case EAIWerewolfRuntimeLanguage::Chinese:
        SetRuntimeLanguage(EAIWerewolfRuntimeLanguage::Bilingual);
        break;
    default:
        SetRuntimeLanguage(EAIWerewolfRuntimeLanguage::English);
        break;
    }

    const FString Message = FString::Printf(TEXT("%s: %s."),
        *LocalizeRuntimeText(TEXT("Language"), TEXT("语言")),
        *GetRuntimeLanguageLabel());
    SetGameLog(Message);
    if (RuntimeChatScrollBox)
    {
        AppendRuntimeChatMessage(TEXT("System / 系统"), Message, FLinearColor(0.55f, 0.70f, 0.95f, 1.0f));
    }
}

void AWinyunqDemoGameMode::RefreshRuntimeLanguageLabels()
{
    const FString LanguageLabel = FString::Printf(TEXT("%s: %s"),
        *LocalizeRuntimeText(TEXT("Lang"), TEXT("语言")),
        *GetRuntimeLanguageLabel());

    if (SetupLanguageButtonText)
    {
        SetupLanguageButtonText->SetText(FText::FromString(LanguageLabel));
    }
    else
    {
        SetTextBlock(TEXT("LanguageText"), LanguageLabel);
    }

    if (RuntimeLanguageButtonText)
    {
        RuntimeLanguageButtonText->SetText(FText::FromString(LanguageLabel));
    }
}

FString AWinyunqDemoGameMode::LocalizeRuntimeText(const FString& EnglishText, const FString& ChineseText) const
{
    switch (RuntimeLanguage)
    {
    case EAIWerewolfRuntimeLanguage::English:
        return EnglishText;
    case EAIWerewolfRuntimeLanguage::Chinese:
        return ChineseText;
    default:
        return FString::Printf(TEXT("%s / %s"), *EnglishText, *ChineseText);
    }
}

FString AWinyunqDemoGameMode::GetRuntimeLanguageLabel() const
{
    switch (RuntimeLanguage)
    {
    case EAIWerewolfRuntimeLanguage::English:
        return TEXT("English");
    case EAIWerewolfRuntimeLanguage::Chinese:
        return TEXT("中文");
    default:
        return TEXT("Bilingual / 双语");
    }
}

FString AWinyunqDemoGameMode::GetRuntimeLanguagePromptInstruction() const
{
    switch (RuntimeLanguage)
    {
    case EAIWerewolfRuntimeLanguage::English:
        return TEXT("Output language: English only. Keep names like P1, P2 unchanged.");
    case EAIWerewolfRuntimeLanguage::Chinese:
        return TEXT("Output language: Simplified Chinese only. Keep player ids like P1, P2 unchanged. Use concise Chinese speech.");
    default:
        return TEXT("Output language: bilingual English and Simplified Chinese. Keep player ids like P1, P2 unchanged. For speeches, put English first, then Chinese.");
    }
}

FString AWinyunqDemoGameMode::BuildRuntimeAutoPlayerSpeech() const
{
    switch (RuntimeLanguage)
    {
    case EAIWerewolfRuntimeLanguage::English:
        return TEXT("I am watching the vote pattern and will stay concise. I want each AI to explain one suspicion clearly.");
    case EAIWerewolfRuntimeLanguage::Chinese:
        return TEXT("我会观察投票和发言顺序。请每个 AI 明确说出一个怀疑点，不要只给空话。");
    default:
        return TEXT("I am watching the vote pattern and want clear suspicions. / 我会观察投票和发言顺序，请每个 AI 明确说出一个怀疑点。");
    }
}

int32 AWinyunqDemoGameMode::ChooseRuntimeAutoPlayerVoteTarget() const
{
    for (int32 PlayerIndex = 0; PlayerIndex < RuntimePlayers.Num(); ++PlayerIndex)
    {
        if (IsRuntimeVoteTargetValid(PlayerIndex, RuntimeHumanPlayerIndex) && !RuntimePlayers[PlayerIndex].bHuman)
        {
            return PlayerIndex;
        }
    }

    return INDEX_NONE;
}

void AWinyunqDemoGameMode::StartRuntimeAutoPlayTimerIfNeeded()
{
    if (!bRuntimeAutoPlayHuman || !GetWorld())
    {
        return;
    }

    if (!GetWorldTimerManager().IsTimerActive(RuntimeAutoPlayTimerHandle))
    {
        GetWorldTimerManager().SetTimer(
            RuntimeAutoPlayTimerHandle,
            this,
            &AWinyunqDemoGameMode::HandleRuntimeAutoPlayTick,
            0.75f,
            true,
            0.75f);
    }
}

void AWinyunqDemoGameMode::StopRuntimeAutoPlayTimer()
{
    if (GetWorld())
    {
        GetWorldTimerManager().ClearTimer(RuntimeAutoPlayTimerHandle);
    }
}

void AWinyunqDemoGameMode::HandleRuntimeAutoPlayTick()
{
    if (!bRuntimeAutoPlayHuman)
    {
        StopRuntimeAutoPlayTimer();
        return;
    }

    if (RuntimePhase == EAIWerewolfRuntimePhase::Ended)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AIWerewolf] Auto play reached game end at round %d."), RuntimeRoundIndex);
        StopRuntimeAutoPlayTimer();
        return;
    }

    if (RuntimeRoundIndex >= RuntimeAutoPlayMaxRounds && RuntimePhase == EAIWerewolfRuntimePhase::Results)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AIWerewolf] Auto play completed requested rounds. Round=%d MaxRounds=%d"),
            RuntimeRoundIndex,
            RuntimeAutoPlayMaxRounds);
        StopRuntimeAutoPlayTimer();
        return;
    }

    if (bRuntimeAIRequestInFlight || RuntimePendingAIPlayers.Num() > 0)
    {
        return;
    }

    if (bRuntimeWaitingForHumanSpeech && RuntimeActiveAIPlayerIndex == RuntimeHumanPlayerIndex)
    {
        if (RuntimePlayerInput)
        {
            RuntimePlayerInput->SetText(FText::FromString(BuildRuntimeAutoPlayerSpeech()));
        }
        UE_LOG(LogTemp, Warning, TEXT("[AIWerewolf] Auto play submitting human speech. Round=%d"), RuntimeRoundIndex);
        HandleRuntimeSendClicked();
        return;
    }

    if (RuntimePhase == EAIWerewolfRuntimePhase::Discussion && !bRuntimeWaitingForHumanSpeech)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AIWerewolf] Auto play advancing discussion to voting. Round=%d"), RuntimeRoundIndex);
        EnterRuntimeVotingPhase();
        return;
    }

    if (RuntimePhase == EAIWerewolfRuntimePhase::Voting)
    {
        if (HumanVoteTarget == INDEX_NONE)
        {
            const int32 TargetIndex = ChooseRuntimeAutoPlayerVoteTarget();
            if (TargetIndex != INDEX_NONE)
            {
                UE_LOG(LogTemp, Warning, TEXT("[AIWerewolf] Auto play choosing human vote target P%d."), TargetIndex + 1);
                HandleRuntimeVoteClicked(TargetIndex);
            }
        }

        if (HumanVoteTarget != INDEX_NONE)
        {
            UE_LOG(LogTemp, Warning, TEXT("[AIWerewolf] Auto play resolving vote. Round=%d"), RuntimeRoundIndex);
            ResolveRuntimeVote();
        }
        return;
    }

    if (RuntimePhase == EAIWerewolfRuntimePhase::Results)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AIWerewolf] Auto play advancing to next night. Round=%d"), RuntimeRoundIndex);
        AdvanceRuntimeWerewolfPhase();
    }
}

UTextBlock* AWinyunqDemoGameMode::CreateRuntimeText(
    FName WidgetName,
    const FString& Text,
    int32 FontSize,
    const FLinearColor& Color,
    bool bAutoWrap) const
{
    UTextBlock* TextBlock = ConstructRuntimeWidget<UTextBlock>(ActiveAIWerewolfWidget, WidgetName);
    if (!TextBlock)
    {
        return nullptr;
    }

    TextBlock->SetText(FText::FromString(Text));
    TextBlock->SetColorAndOpacity(FSlateColor(Color));
    TextBlock->SetAutoWrapText(bAutoWrap);

    FSlateFontInfo Font = TextBlock->GetFont();
    Font.Size = FontSize;
    TextBlock->SetFont(Font);
    return TextBlock;
}

void AWinyunqDemoGameMode::BindRuntimeButton(UButton* Button, FName HandlerName)
{
    if (!Button)
    {
        return;
    }

    Button->OnClicked.Clear();
    FScriptDelegate Delegate;
    Delegate.BindUFunction(this, HandlerName);
    Button->OnClicked.AddUnique(Delegate);
    Button->SetTouchMethod(EButtonTouchMethod::PreciseTap);
    Button->SetClickMethod(EButtonClickMethod::PreciseClick);
    Button->SetIsEnabled(true);
}

UButton* AWinyunqDemoGameMode::CreateRuntimeButton(
    FName WidgetName,
    const FString& Label,
    FName HandlerName,
    const FLinearColor& ButtonColor,
    UTextBlock** OutLabelText)
{
    UButton* Button = ConstructRuntimeWidget<UButton>(ActiveAIWerewolfWidget, WidgetName);
    if (!Button)
    {
        return nullptr;
    }

    Button->SetBackgroundColor(ButtonColor);
    BindRuntimeButton(Button, HandlerName);

    UTextBlock* LabelText = CreateRuntimeText(
        FName(*(WidgetName.ToString() + TEXT("_Label"))),
        Label,
        17,
        FLinearColor::White,
        true);
    if (LabelText)
    {
        LabelText->SetJustification(ETextJustify::Center);
        Button->SetContent(LabelText);
    }

    if (OutLabelText)
    {
        *OutLabelText = LabelText;
    }

    return Button;
}

void AWinyunqDemoGameMode::BuildRuntimeWerewolfGameUI()
{
    if (RuntimeGamePanel || !ActiveAIWerewolfWidget)
    {
        return;
    }

    UPanelWidget* RootPanel = Cast<UPanelWidget>(ActiveAIWerewolfWidget->GetWidgetFromName(TEXT("RootCanvas")));
    if (!RootPanel && ActiveAIWerewolfWidget->WidgetTree)
    {
        RootPanel = Cast<UPanelWidget>(ActiveAIWerewolfWidget->WidgetTree->RootWidget);
    }

    if (!RootPanel)
    {
        SetGameLog(TEXT("Cannot build game UI: WBP root panel was not found."));
        return;
    }

    RuntimeGamePanel = ConstructRuntimeWidget<UCanvasPanel>(ActiveAIWerewolfWidget, TEXT("RuntimeWerewolfGamePanel"));
    if (!RuntimeGamePanel)
    {
        SetGameLog(TEXT("Cannot build game UI: failed to create RuntimeWerewolfGamePanel."));
        return;
    }

    RootPanel->AddChild(RuntimeGamePanel);
    if (UCanvasPanelSlot* GamePanelSlot = Cast<UCanvasPanelSlot>(RuntimeGamePanel->Slot))
    {
        GamePanelSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
        GamePanelSlot->SetOffsets(FMargin(0.0f));
    }
    RuntimeGamePanel->SetVisibility(ESlateVisibility::Collapsed);

    UBorder* Background = ConstructRuntimeWidget<UBorder>(ActiveAIWerewolfWidget, TEXT("RuntimeGameBackground"));
    Background->SetBrushColor(FLinearColor(0.035f, 0.044f, 0.055f, 1.0f));
    RuntimeGamePanel->AddChildToCanvas(Background);
    if (UCanvasPanelSlot* BackgroundSlot = Cast<UCanvasPanelSlot>(Background->Slot))
    {
        BackgroundSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
        BackgroundSlot->SetOffsets(FMargin(0.0f));
    }

    UVerticalBox* MainBox = ConstructRuntimeWidget<UVerticalBox>(ActiveAIWerewolfWidget, TEXT("RuntimeMainBox"));
    RuntimeGamePanel->AddChildToCanvas(MainBox);
    if (UCanvasPanelSlot* MainSlot = Cast<UCanvasPanelSlot>(MainBox->Slot))
    {
        MainSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
        MainSlot->SetOffsets(FMargin(18.0f, 14.0f, 18.0f, 14.0f));
    }

    UHorizontalBox* HeaderBox = ConstructRuntimeWidget<UHorizontalBox>(ActiveAIWerewolfWidget, TEXT("RuntimeHeaderBox"));
    if (UVerticalBoxSlot* HeaderSlot = MainBox->AddChildToVerticalBox(HeaderBox))
    {
        HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
        HeaderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
    }

    UTextBlock* TitleText = CreateRuntimeText(
        TEXT("RuntimeTitleText"),
        TEXT("AI Werewolf / AI 狼人杀"),
        28,
        FLinearColor(0.96f, 0.97f, 0.99f, 1.0f));
    if (UHorizontalBoxSlot* TitleSlot = HeaderBox->AddChildToHorizontalBox(TitleText))
    {
        TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        TitleSlot->SetVerticalAlignment(VAlign_Center);
    }

    RuntimePhaseText = CreateRuntimeText(
        TEXT("RuntimePhaseText"),
        TEXT("Phase / 阶段"),
        18,
        FLinearColor(0.75f, 0.82f, 0.92f, 1.0f),
        true);
    RuntimePhaseText->SetJustification(ETextJustify::Center);
    if (UHorizontalBoxSlot* PhaseSlot = HeaderBox->AddChildToHorizontalBox(RuntimePhaseText))
    {
        PhaseSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        PhaseSlot->SetVerticalAlignment(VAlign_Center);
    }

    UTextBlock* RuntimeLanguageLabel = nullptr;
    UButton* RuntimeLanguageButton = CreateRuntimeButton(
        TEXT("RuntimeLanguageButton"),
        TEXT("Language / 语言"),
        GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleLanguageToggleClicked),
        FLinearColor(0.18f, 0.24f, 0.34f, 1.0f),
        &RuntimeLanguageLabel);
    RuntimeLanguageButtonText = RuntimeLanguageLabel;
    if (UHorizontalBoxSlot* LanguageSlot = HeaderBox->AddChildToHorizontalBox(RuntimeLanguageButton))
    {
        LanguageSlot->SetPadding(FMargin(8.0f, 0.0f));
        LanguageSlot->SetVerticalAlignment(VAlign_Center);
    }

    UButton* BackButton = CreateRuntimeButton(
        TEXT("RuntimeBackSetupButton"),
        TEXT("Setup / 设置"),
        GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleRuntimeBackSetupClicked),
        FLinearColor(0.20f, 0.24f, 0.30f, 1.0f));
    if (UHorizontalBoxSlot* BackSlot = HeaderBox->AddChildToHorizontalBox(BackButton))
    {
        BackSlot->SetPadding(FMargin(8.0f, 0.0f));
        BackSlot->SetVerticalAlignment(VAlign_Center);
    }

    UButton* ResetButton = CreateRuntimeButton(
        TEXT("RuntimeResetButton"),
        TEXT("Reset / 重开"),
        GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleRuntimeResetClicked),
        FLinearColor(0.38f, 0.23f, 0.20f, 1.0f));
    if (UHorizontalBoxSlot* ResetSlot = HeaderBox->AddChildToHorizontalBox(ResetButton))
    {
        ResetSlot->SetPadding(FMargin(8.0f, 0.0f));
        ResetSlot->SetVerticalAlignment(VAlign_Center);
    }

    UTextBlock* NextPhaseLabel = nullptr;
    UButton* NextPhaseButton = CreateRuntimeButton(
        TEXT("RuntimeNextPhaseButton"),
        TEXT("Next / 下一步"),
        GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleRuntimeNextPhaseClicked),
        FLinearColor(0.20f, 0.42f, 0.72f, 1.0f),
        &NextPhaseLabel);
    RuntimeNextPhaseButtonText = NextPhaseLabel;
    RuntimeNextPhaseButton = NextPhaseButton;
    if (UHorizontalBoxSlot* NextSlot = HeaderBox->AddChildToHorizontalBox(NextPhaseButton))
    {
        NextSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
        NextSlot->SetVerticalAlignment(VAlign_Center);
    }

    UHorizontalBox* BodyBox = ConstructRuntimeWidget<UHorizontalBox>(ActiveAIWerewolfWidget, TEXT("RuntimeBodyBox"));
    if (UVerticalBoxSlot* BodySlot = MainBox->AddChildToVerticalBox(BodyBox))
    {
        BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    UBorder* PlayerPanel = ConstructRuntimeWidget<UBorder>(ActiveAIWerewolfWidget, TEXT("RuntimePlayerPanel"));
    PlayerPanel->SetPadding(FMargin(12.0f));
    PlayerPanel->SetBrushColor(FLinearColor(0.09f, 0.11f, 0.14f, 0.96f));
    if (UHorizontalBoxSlot* PlayerPanelSlot = BodyBox->AddChildToHorizontalBox(PlayerPanel))
    {
        PlayerPanelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        PlayerPanelSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
    }

    UVerticalBox* PlayerPanelBox = ConstructRuntimeWidget<UVerticalBox>(ActiveAIWerewolfWidget, TEXT("RuntimePlayerPanelBox"));
    PlayerPanel->SetContent(PlayerPanelBox);

    UTextBlock* PlayerTitle = CreateRuntimeText(
        TEXT("RuntimePlayerPanelTitle"),
        TEXT("Players / 玩家"),
        20,
        FLinearColor(0.95f, 0.96f, 0.98f, 1.0f));
    if (UVerticalBoxSlot* PlayerTitleSlot = PlayerPanelBox->AddChildToVerticalBox(PlayerTitle))
    {
        PlayerTitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
    }

    UUniformGridPanel* PlayerGrid = ConstructRuntimeWidget<UUniformGridPanel>(ActiveAIWerewolfWidget, TEXT("RuntimePlayerGrid"));
    if (UVerticalBoxSlot* PlayerGridSlot = PlayerPanelBox->AddChildToVerticalBox(PlayerGrid))
    {
        PlayerGridSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    RuntimePlayerButtons.SetNum(MaxRuntimePlayers);
    RuntimePlayerAvatarTexts.SetNum(MaxRuntimePlayers);
    RuntimePlayerNameTexts.SetNum(MaxRuntimePlayers);
    RuntimePlayerRoleTexts.SetNum(MaxRuntimePlayers);
    RuntimePlayerStatusTexts.SetNum(MaxRuntimePlayers);

    static const FName VoteHandlers[MaxRuntimePlayers] =
    {
        GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleRuntimeVotePlayer1Clicked),
        GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleRuntimeVotePlayer2Clicked),
        GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleRuntimeVotePlayer3Clicked),
        GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleRuntimeVotePlayer4Clicked),
        GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleRuntimeVotePlayer5Clicked),
        GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleRuntimeVotePlayer6Clicked),
        GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleRuntimeVotePlayer7Clicked),
        GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleRuntimeVotePlayer8Clicked),
        GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleRuntimeVotePlayer9Clicked),
        GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleRuntimeVotePlayer10Clicked)
    };

    for (int32 PlayerIndex = 0; PlayerIndex < MaxRuntimePlayers; ++PlayerIndex)
    {
        UButton* PlayerButton = ConstructRuntimeWidget<UButton>(
            ActiveAIWerewolfWidget,
            FName(*FString::Printf(TEXT("RuntimePlayerButton_%d"), PlayerIndex + 1)));
        PlayerButton->SetBackgroundColor(FLinearColor(0.12f, 0.14f, 0.18f, 1.0f));
        BindRuntimeButton(PlayerButton, VoteHandlers[PlayerIndex]);
        RuntimePlayerButtons[PlayerIndex] = PlayerButton;

        UBorder* CardBorder = ConstructRuntimeWidget<UBorder>(
            ActiveAIWerewolfWidget,
            FName(*FString::Printf(TEXT("RuntimePlayerCard_%d"), PlayerIndex + 1)));
        CardBorder->SetPadding(FMargin(8.0f));
        CardBorder->SetBrushColor(FLinearColor(0.13f, 0.16f, 0.20f, 1.0f));
        PlayerButton->SetContent(CardBorder);

        UVerticalBox* CardBox = ConstructRuntimeWidget<UVerticalBox>(
            ActiveAIWerewolfWidget,
            FName(*FString::Printf(TEXT("RuntimePlayerCardBox_%d"), PlayerIndex + 1)));
        CardBorder->SetContent(CardBox);

        UBorder* AvatarBorder = ConstructRuntimeWidget<UBorder>(
            ActiveAIWerewolfWidget,
            FName(*FString::Printf(TEXT("RuntimePlayerAvatar_%d"), PlayerIndex + 1)));
        AvatarBorder->SetPadding(FMargin(10.0f));
        AvatarBorder->SetBrushColor(GetRuntimeAvatarColor(PlayerIndex));
        if (UVerticalBoxSlot* AvatarSlot = CardBox->AddChildToVerticalBox(AvatarBorder))
        {
            AvatarSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
            AvatarSlot->SetHorizontalAlignment(HAlign_Fill);
        }

        UTextBlock* AvatarText = CreateRuntimeText(
            FName(*FString::Printf(TEXT("RuntimePlayerAvatarText_%d"), PlayerIndex + 1)),
            FString::Printf(TEXT("P%d"), PlayerIndex + 1),
            22,
            FLinearColor::White);
        AvatarText->SetJustification(ETextJustify::Center);
        AvatarBorder->SetContent(AvatarText);
        RuntimePlayerAvatarTexts[PlayerIndex] = AvatarText;

        UTextBlock* NameText = CreateRuntimeText(
            FName(*FString::Printf(TEXT("RuntimePlayerNameText_%d"), PlayerIndex + 1)),
            FString::Printf(TEXT("Player %d"), PlayerIndex + 1),
            16,
            FLinearColor(0.96f, 0.96f, 0.98f, 1.0f),
            true);
        NameText->SetJustification(ETextJustify::Center);
        CardBox->AddChildToVerticalBox(NameText);
        RuntimePlayerNameTexts[PlayerIndex] = NameText;

        UTextBlock* RoleText = CreateRuntimeText(
            FName(*FString::Printf(TEXT("RuntimePlayerRoleText_%d"), PlayerIndex + 1)),
            TEXT("Role hidden"),
            13,
            FLinearColor(0.70f, 0.75f, 0.82f, 1.0f),
            true);
        RoleText->SetJustification(ETextJustify::Center);
        CardBox->AddChildToVerticalBox(RoleText);
        RuntimePlayerRoleTexts[PlayerIndex] = RoleText;

        UTextBlock* StatusText = CreateRuntimeText(
            FName(*FString::Printf(TEXT("RuntimePlayerStatusText_%d"), PlayerIndex + 1)),
            TEXT("Alive"),
            13,
            FLinearColor(0.76f, 0.90f, 0.78f, 1.0f),
            true);
        StatusText->SetJustification(ETextJustify::Center);
        CardBox->AddChildToVerticalBox(StatusText);
        RuntimePlayerStatusTexts[PlayerIndex] = StatusText;

        if (UUniformGridSlot* GridSlot = PlayerGrid->AddChildToUniformGrid(PlayerButton, PlayerIndex / 2, PlayerIndex % 2))
        {
            GridSlot->SetHorizontalAlignment(HAlign_Fill);
            GridSlot->SetVerticalAlignment(VAlign_Fill);
        }
    }

    UBorder* GamePanel = ConstructRuntimeWidget<UBorder>(ActiveAIWerewolfWidget, TEXT("RuntimeGamePanelBorder"));
    GamePanel->SetPadding(FMargin(12.0f));
    GamePanel->SetBrushColor(FLinearColor(0.07f, 0.085f, 0.105f, 0.97f));
    if (UHorizontalBoxSlot* GamePanelSlot = BodyBox->AddChildToHorizontalBox(GamePanel))
    {
        GamePanelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    UVerticalBox* GameBox = ConstructRuntimeWidget<UVerticalBox>(ActiveAIWerewolfWidget, TEXT("RuntimeGameBox"));
    GamePanel->SetContent(GameBox);

    RuntimeInstructionText = CreateRuntimeText(
        TEXT("RuntimeInstructionText"),
        TEXT("Instruction / 操作提示"),
        17,
        FLinearColor(0.84f, 0.88f, 0.95f, 1.0f),
        true);
    if (UVerticalBoxSlot* InstructionSlot = GameBox->AddChildToVerticalBox(RuntimeInstructionText))
    {
        InstructionSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
    }

    UBorder* ChatBorder = ConstructRuntimeWidget<UBorder>(ActiveAIWerewolfWidget, TEXT("RuntimeChatBorder"));
    ChatBorder->SetPadding(FMargin(8.0f));
    ChatBorder->SetBrushColor(FLinearColor(0.04f, 0.05f, 0.065f, 1.0f));
    if (UVerticalBoxSlot* ChatSlot = GameBox->AddChildToVerticalBox(ChatBorder))
    {
        ChatSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        ChatSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
    }

    RuntimeChatScrollBox = ConstructRuntimeWidget<UScrollBox>(ActiveAIWerewolfWidget, TEXT("RuntimeChatScrollBox"));
    ChatBorder->SetContent(RuntimeChatScrollBox);

    RuntimeVoteBox = ConstructRuntimeWidget<UVerticalBox>(ActiveAIWerewolfWidget, TEXT("RuntimeVoteBox"));
    if (UVerticalBoxSlot* VoteSlot = GameBox->AddChildToVerticalBox(RuntimeVoteBox))
    {
        VoteSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        VoteSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
    }

    UHorizontalBox* InputBox = ConstructRuntimeWidget<UHorizontalBox>(ActiveAIWerewolfWidget, TEXT("RuntimeInputBox"));
    if (UVerticalBoxSlot* InputSlot = GameBox->AddChildToVerticalBox(InputBox))
    {
        InputSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
    }

    RuntimePlayerInput = ConstructRuntimeWidget<UEditableTextBox>(ActiveAIWerewolfWidget, TEXT("RuntimePlayerInput"));
    RuntimePlayerInput->SetHintText(FText::FromString(LocalizeRuntimeText(TEXT("Type your speech"), TEXT("输入你的发言"))));
    RuntimePlayerInput->SetClearKeyboardFocusOnCommit(false);
    if (UHorizontalBoxSlot* TextInputSlot = InputBox->AddChildToHorizontalBox(RuntimePlayerInput))
    {
        TextInputSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        TextInputSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
        TextInputSlot->SetVerticalAlignment(VAlign_Center);
    }

    UButton* SendButton = CreateRuntimeButton(
        TEXT("RuntimeSendButton"),
        TEXT("Speak / 发言"),
        GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleRuntimeSendClicked),
        FLinearColor(0.17f, 0.48f, 0.37f, 1.0f));
    if (UHorizontalBoxSlot* SendSlot = InputBox->AddChildToHorizontalBox(SendButton))
    {
        SendSlot->SetVerticalAlignment(VAlign_Center);
    }

    RefreshRuntimeLanguageLabels();
}

void AWinyunqDemoGameMode::ShowRuntimeWerewolfGameUI(bool bShow)
{
    if (bShow)
    {
        BuildRuntimeWerewolfGameUI();
    }

    UPanelWidget* RootPanel = Cast<UPanelWidget>(ActiveAIWerewolfWidget ? ActiveAIWerewolfWidget->GetWidgetFromName(TEXT("RootCanvas")) : nullptr);
    if (!RootPanel && ActiveAIWerewolfWidget && ActiveAIWerewolfWidget->WidgetTree)
    {
        RootPanel = Cast<UPanelWidget>(ActiveAIWerewolfWidget->WidgetTree->RootWidget);
    }

    if (RootPanel)
    {
        for (int32 ChildIndex = 0; ChildIndex < RootPanel->GetChildrenCount(); ++ChildIndex)
        {
            if (UWidget* Child = RootPanel->GetChildAt(ChildIndex))
            {
                if (Child != RuntimeGamePanel)
                {
                    Child->SetVisibility(bShow ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
                }
            }
        }
    }

    if (RuntimeGamePanel)
    {
        RuntimeGamePanel->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

void AWinyunqDemoGameMode::StartRuntimeWerewolfGame()
{
    ShowRuntimeWerewolfGameUI(true);
    ResetRuntimeWerewolfGame();
    StartRuntimeAutoPlayTimerIfNeeded();
}

void AWinyunqDemoGameMode::ResetRuntimeWerewolfGame()
{
    ShowRuntimeWerewolfGameUI(true);
    if (bRuntimeAIRequestInFlight)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AIWerewolf] Reset requested while AI request #%d is in flight; stopping inference."),
            RuntimeActiveAIRequestSerial);
        ULiteRtLmBlueprintLibrary::StopLiteRtLmInference();
    }
    StopRuntimeAIWatchdog();
    ReleaseRuntimeAISessions();

    RuntimeRoundIndex = 0;
    RuntimeHumanPlayerIndex = INDEX_NONE;
    RuntimeDiscussionTurnCursor = 0;
    HumanVoteTarget = INDEX_NONE;
    RuntimeActiveAIPlayerIndex = INDEX_NONE;
    RuntimePhase = EAIWerewolfRuntimePhase::Setup;
    RuntimeAIRequestType = EAIWerewolfRuntimeAIRequest::None;
    bRuntimeAIRequestInFlight = false;
    bRuntimeWaitingForHumanSpeech = false;
    RuntimeAIRequestStartTimeSeconds = 0.0;
    RuntimeActiveAIRequestSerial = 0;
    RuntimeAIReceivedChunkCount = 0;
    RuntimeAIReceivedCharCount = 0;
    bRuntimeAIStopRequested = false;
    bRuntimeAIStallWarningShown = false;
    RuntimeActiveAIResponse.Reset();
    RuntimePlayers.Reset();
    RuntimePendingAIPlayers.Reset();
    RuntimeVoteCounts.Reset();
    RuntimeTranscript.Reset();

    const int32 ClampedPlayerCount = FMath::Clamp(SelectedPlayerCount, 5, MaxRuntimePlayers);
    TArray<FString> AIRoles;

    const int32 WerewolfCount = ClampedPlayerCount >= 9 ? 3 : (ClampedPlayerCount >= 7 ? 2 : 1);
    for (int32 Index = 0; Index < WerewolfCount; ++Index)
    {
        AIRoles.Add(TEXT("Werewolf"));
    }
    AIRoles.Add(TEXT("Seer"));
    if (ClampedPlayerCount >= 7)
    {
        AIRoles.Add(TEXT("Witch"));
    }
    if (ClampedPlayerCount >= 9)
    {
        AIRoles.Add(TEXT("Guard"));
    }
    while (AIRoles.Num() < ClampedPlayerCount - 1)
    {
        AIRoles.Add(TEXT("Villager"));
    }

    for (int32 Index = 0; Index < AIRoles.Num(); ++Index)
    {
        const int32 SwapIndex = FMath::RandRange(Index, AIRoles.Num() - 1);
        AIRoles.Swap(Index, SwapIndex);
    }

    for (int32 PlayerIndex = 0; PlayerIndex < ClampedPlayerCount; ++PlayerIndex)
    {
        const bool bIsHumanPlayer = PlayerIndex == ClampedPlayerCount - 1;
        FAIWerewolfRuntimePlayer Player;
        Player.Name = bIsHumanPlayer ? TEXT("You / 玩家") : FString::Printf(TEXT("AI-%02d"), PlayerIndex + 1);
        Player.Role = bIsHumanPlayer ? TEXT("Villager") : AIRoles[PlayerIndex];
        Player.AvatarColor = GetRuntimeAvatarColor(PlayerIndex);
        Player.bAlive = true;
        Player.bHuman = bIsHumanPlayer;
        if (bIsHumanPlayer)
        {
            RuntimeHumanPlayerIndex = PlayerIndex;
        }
        RuntimePlayers.Add(Player);
        RuntimeAISessionOwners.Add(NewObject<UAIWerewolfRuntimeSessionOwner>(this));
    }

    if (RuntimeChatScrollBox)
    {
        RuntimeChatScrollBox->ClearChildren();
    }

    RefreshRuntimePlayers();
    RebuildRuntimeVoteButtons();

    AppendRuntimeChatMessage(
        TEXT("System / 系统"),
        FString::Printf(TEXT("New game started with %d players. You are P%d in the last seat. Your visible role is Villager."), ClampedPlayerCount, RuntimeHumanPlayerIndex + 1),
        FLinearColor(0.55f, 0.70f, 0.95f, 1.0f));

    AppendRuntimeChatMessage(
        TEXT("System / 系统"),
        ULiteRtLmBlueprintLibrary::IsLiteRtLmModelLoaded()
            ? TEXT("LiteRT-LM model is loaded. AI speech and votes will use local model inference.")
            : TEXT("LiteRT-LM model is not loaded. This demo will not fake AI dialogue; go back and load the model first."),
        FLinearColor(0.55f, 0.70f, 0.95f, 1.0f));

    if (!ULiteRtLmBlueprintLibrary::IsLiteRtLmModelLoaded())
    {
        RefreshRuntimePhaseText();
        RebuildRuntimeVoteButtons();
        return;
    }

    RunRuntimeNightPhase();
}

void AWinyunqDemoGameMode::ReleaseRuntimeAISessions()
{
    for (UObject* SessionOwner : RuntimeAISessionOwners)
    {
        if (IsValid(SessionOwner))
        {
            ULiteRtLmBlueprintLibrary::ReleaseLiteRtLmSession(this, SessionOwner);
        }
    }
    RuntimeAISessionOwners.Reset();
}

void AWinyunqDemoGameMode::ReturnToSetupUI()
{
    if (bRuntimeAIRequestInFlight)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AIWerewolf] Return to setup requested while AI request #%d is in flight; stopping inference."),
            RuntimeActiveAIRequestSerial);
        ULiteRtLmBlueprintLibrary::StopLiteRtLmInference();
    }
    StopRuntimeAIWatchdog();
    StopRuntimeAutoPlayTimer();
    RuntimePhase = EAIWerewolfRuntimePhase::Setup;
    bRuntimeWaitingForHumanSpeech = false;
    bRuntimeAIRequestInFlight = false;
    RuntimeAIRequestType = EAIWerewolfRuntimeAIRequest::None;
    RuntimeActiveAIPlayerIndex = INDEX_NONE;
    ShowRuntimeWerewolfGameUI(false);
    SetGameLog(TEXT("Returned to setup / 已返回设置界面."));
}

void AWinyunqDemoGameMode::AdvanceRuntimeWerewolfPhase()
{
    if (bRuntimeAIRequestInFlight || RuntimePendingAIPlayers.Num() > 0 || bRuntimeWaitingForHumanSpeech)
    {
        AppendRuntimeChatMessage(
            TEXT("System / 系统"),
            bRuntimeWaitingForHumanSpeech
                ? TEXT("It is your speech turn. Type a message and tap Speak to continue.")
                : TEXT("AI is still thinking. The game will continue automatically when the request finishes."),
            FLinearColor(0.93f, 0.72f, 0.30f, 1.0f));
        return;
    }

    switch (RuntimePhase)
    {
    case EAIWerewolfRuntimePhase::Setup:
        StartRuntimeWerewolfGame();
        break;
    case EAIWerewolfRuntimePhase::Night:
        RunRuntimeDiscussionPhase();
        break;
    case EAIWerewolfRuntimePhase::Discussion:
        EnterRuntimeVotingPhase();
        break;
    case EAIWerewolfRuntimePhase::Voting:
        ResolveRuntimeVote();
        break;
    case EAIWerewolfRuntimePhase::Results:
        RunRuntimeNightPhase();
        if (RuntimePhase != EAIWerewolfRuntimePhase::Ended)
        {
            RunRuntimeDiscussionPhase();
        }
        break;
    case EAIWerewolfRuntimePhase::Ended:
        ResetRuntimeWerewolfGame();
        break;
    default:
        break;
    }
}

void AWinyunqDemoGameMode::RunRuntimeNightPhase()
{
    RuntimePhase = EAIWerewolfRuntimePhase::Night;
    ++RuntimeRoundIndex;
    HumanVoteTarget = INDEX_NONE;
    RuntimeDiscussionTurnCursor = 0;
    bRuntimeWaitingForHumanSpeech = false;
    RuntimePendingAIPlayers.Reset();
    RuntimeVoteCounts.Reset();
    RefreshRuntimePhaseText();
    RebuildRuntimeVoteButtons();

    AppendRuntimeChatMessage(
        TEXT("System / 系统"),
        FString::Printf(TEXT("Night %d begins. Werewolves choose a target."), RuntimeRoundIndex),
        FLinearColor(0.42f, 0.52f, 0.82f, 1.0f));

    int32 ActingWerewolfIndex = INDEX_NONE;
    for (int32 PlayerIndex = 0; PlayerIndex < RuntimePlayers.Num(); ++PlayerIndex)
    {
        if (RuntimePlayers[PlayerIndex].bAlive && RuntimePlayers[PlayerIndex].IsWerewolf())
        {
            ActingWerewolfIndex = PlayerIndex;
            break;
        }
    }

    if (StartRuntimeAIRequest(EAIWerewolfRuntimeAIRequest::NightKill, ActingWerewolfIndex, BuildRuntimeNightPrompt(ActingWerewolfIndex)))
    {
        return;
    }

    AppendRuntimeChatMessage(
        TEXT("System / 系统"),
        TEXT("LiteRT-LM model is not available, so night cannot resolve. Load the model and start a new game."),
        FLinearColor(0.93f, 0.42f, 0.36f, 1.0f));
}

void AWinyunqDemoGameMode::CompleteRuntimeNightPhaseFromAI(const FString& AIText)
{
    int32 TargetIndex = ParseRuntimeTargetIndexFromText(AIText);
    if (!RuntimePlayers.IsValidIndex(TargetIndex) || !RuntimePlayers[TargetIndex].bAlive || RuntimePlayers[TargetIndex].IsWerewolf())
    {
        AppendRuntimeChatMessage(
            TEXT("System / 系统"),
            TEXT("LiteRT-LM did not return a valid MCP night target. No one was eliminated tonight."),
            FLinearColor(0.93f, 0.72f, 0.30f, 1.0f));
        RefreshRuntimePlayers();
        if (!EvaluateRuntimeWinCondition())
        {
            RunRuntimeDiscussionPhase();
        }
        return;
    }

    if (RuntimePlayers.IsValidIndex(TargetIndex))
    {
        RuntimePlayers[TargetIndex].bAlive = false;
        AppendRuntimeChatMessage(
            TEXT("System / 系统"),
            FString::Printf(TEXT("Dawn breaks. %s was eliminated at night. Role: %s."), *RuntimePlayers[TargetIndex].Name, *GetRuntimeRoleDisplay(RuntimePlayers[TargetIndex])),
            FLinearColor(0.90f, 0.40f, 0.36f, 1.0f));
    }

    RefreshRuntimePlayers();
    if (!EvaluateRuntimeWinCondition())
    {
        RunRuntimeDiscussionPhase();
    }
}

void AWinyunqDemoGameMode::RunRuntimeDiscussionPhase()
{
    if (RuntimePhase == EAIWerewolfRuntimePhase::Ended)
    {
        return;
    }

    RuntimePhase = EAIWerewolfRuntimePhase::Discussion;
    HumanVoteTarget = INDEX_NONE;
    RuntimePendingAIPlayers.Reset();
    RuntimeDiscussionTurnCursor = 0;
    bRuntimeWaitingForHumanSpeech = false;
    RefreshRuntimePhaseText();
    RebuildRuntimeVoteButtons();

    AppendRuntimeChatMessage(
        TEXT("System / 系统"),
        TEXT("Day discussion starts. Players speak in seat order. Your turn can pause the game; AI turns resolve asynchronously through LiteRT-LM."),
        FLinearColor(0.55f, 0.70f, 0.95f, 1.0f));

    StartNextRuntimeDiscussionTurn();
}

void AWinyunqDemoGameMode::StartNextRuntimeDiscussionTurn()
{
    bRuntimeWaitingForHumanSpeech = false;

    while (RuntimePlayers.IsValidIndex(RuntimeDiscussionTurnCursor) && !RuntimePlayers[RuntimeDiscussionTurnCursor].bAlive)
    {
        ++RuntimeDiscussionTurnCursor;
    }

    if (!RuntimePlayers.IsValidIndex(RuntimeDiscussionTurnCursor))
    {
        bRuntimeAIRequestInFlight = false;
        RuntimeAIRequestType = EAIWerewolfRuntimeAIRequest::None;
        RuntimeActiveAIPlayerIndex = INDEX_NONE;
        AppendRuntimeChatMessage(
            TEXT("System / 系统"),
            TEXT("All living players have spoken. You can start voting now."),
            FLinearColor(0.55f, 0.70f, 0.95f, 1.0f));
        RefreshRuntimePhaseText();
        RefreshRuntimePlayers();
        return;
    }

    const int32 PlayerIndex = RuntimeDiscussionTurnCursor;
    RuntimeActiveAIPlayerIndex = PlayerIndex;

    if (PlayerIndex == RuntimeHumanPlayerIndex)
    {
        bRuntimeWaitingForHumanSpeech = true;
        AppendRuntimeChatMessage(
            TEXT("System / 系统"),
            TEXT("Your speech turn. Type your statement and tap Speak. / 轮到你发言，输入后点击发言。"),
            FLinearColor(0.93f, 0.72f, 0.30f, 1.0f));
        RefreshRuntimePhaseText();
        RefreshRuntimePlayers();
        return;
    }

    if (StartRuntimeAIRequest(EAIWerewolfRuntimeAIRequest::DiscussionSpeech, PlayerIndex, BuildRuntimeDiscussionPrompt(PlayerIndex)))
    {
        RefreshRuntimePlayers();
        return;
    }

    AppendRuntimeChatMessage(
        TEXT("System / 系统"),
        TEXT("LiteRT-LM model is not available, so AI speech cannot continue. Load the model and start a new game."),
        FLinearColor(0.93f, 0.42f, 0.36f, 1.0f));
    RefreshRuntimePhaseText();
    RefreshRuntimePlayers();
}

void AWinyunqDemoGameMode::CompleteRuntimeDiscussionAI(const FString& AIText)
{
    const int32 PlayerIndex = RuntimeActiveAIPlayerIndex;
    FString Speech = AIText.TrimStartAndEnd();
    if (Speech.IsEmpty() || Speech.Len() > 420)
    {
        Speech = TEXT("[LiteRT-LM returned no usable speech for this turn.]");
    }

    if (RuntimePlayers.IsValidIndex(PlayerIndex) && RuntimePlayers[PlayerIndex].bAlive)
    {
        AppendRuntimeChatMessage(RuntimePlayers[PlayerIndex].Name, Speech, RuntimePlayers[PlayerIndex].AvatarColor);
    }

    RuntimeActiveAIPlayerIndex = INDEX_NONE;
    RuntimeAIRequestType = EAIWerewolfRuntimeAIRequest::None;
    bRuntimeAIRequestInFlight = false;
    ++RuntimeDiscussionTurnCursor;
    StartNextRuntimeDiscussionTurn();
}

void AWinyunqDemoGameMode::EnterRuntimeVotingPhase()
{
    if (RuntimePhase == EAIWerewolfRuntimePhase::Ended)
    {
        return;
    }

    RuntimePhase = EAIWerewolfRuntimePhase::Voting;
    HumanVoteTarget = INDEX_NONE;
    RuntimeDiscussionTurnCursor = 0;
    bRuntimeWaitingForHumanSpeech = false;
    RuntimePendingAIPlayers.Reset();
    RuntimeVoteCounts.Reset();
    RefreshRuntimePhaseText();
    RefreshRuntimePlayers();
    RebuildRuntimeVoteButtons();

    AppendRuntimeChatMessage(
        TEXT("System / 系统"),
        TEXT("Voting is open. Tap a player card or a vote button, then tap Resolve Vote."),
        FLinearColor(0.93f, 0.72f, 0.30f, 1.0f));
}

void AWinyunqDemoGameMode::ResolveRuntimeVote()
{
    if (RuntimePhase != EAIWerewolfRuntimePhase::Voting)
    {
        return;
    }

    if (RuntimePlayers.IsValidIndex(RuntimeHumanPlayerIndex) && RuntimePlayers[RuntimeHumanPlayerIndex].bAlive && HumanVoteTarget == INDEX_NONE)
    {
        AppendRuntimeChatMessage(
            TEXT("System / 系统"),
            TEXT("Choose your vote target first. / 请先选择投票目标。"),
            FLinearColor(0.93f, 0.72f, 0.30f, 1.0f));
        return;
    }

    RuntimeVoteCounts.Reset();
    if (HumanVoteTarget != INDEX_NONE)
    {
        RuntimeVoteCounts.FindOrAdd(HumanVoteTarget)++;
    }

    for (int32 PlayerIndex = 0; PlayerIndex < RuntimePlayers.Num(); ++PlayerIndex)
    {
        if (RuntimePlayers[PlayerIndex].bAlive && !RuntimePlayers[PlayerIndex].bHuman)
        {
            RuntimePendingAIPlayers.Add(PlayerIndex);
        }
    }

    AppendRuntimeChatMessage(
        TEXT("System / 系统"),
        TEXT("AI voting starts. Votes will resolve automatically after model callbacks finish."),
        FLinearColor(0.93f, 0.72f, 0.30f, 1.0f));

    StartNextRuntimeVoteAI();
}

void AWinyunqDemoGameMode::StartNextRuntimeVoteAI()
{
    if (RuntimePendingAIPlayers.Num() == 0)
    {
        FinishRuntimeVoteResolution();
        return;
    }

    const int32 PlayerIndex = RuntimePendingAIPlayers[0];
    RuntimePendingAIPlayers.RemoveAt(0);
    RuntimeActiveAIPlayerIndex = PlayerIndex;

    if (StartRuntimeAIRequest(EAIWerewolfRuntimeAIRequest::Vote, PlayerIndex, BuildRuntimeVotePrompt(PlayerIndex)))
    {
        return;
    }

    AppendRuntimeChatMessage(
        TEXT("System / 系统"),
        TEXT("LiteRT-LM model is not available, so AI voting cannot continue. Load the model and start a new game."),
        FLinearColor(0.93f, 0.42f, 0.36f, 1.0f));
}

void AWinyunqDemoGameMode::CompleteRuntimeVoteAI(const FString& AIText)
{
    const int32 VoterIndex = RuntimeActiveAIPlayerIndex;
    int32 TargetIndex = ParseRuntimeTargetIndexFromText(AIText);
    if (!IsRuntimeVoteTargetValid(TargetIndex, VoterIndex))
    {
        AppendRuntimeChatMessage(
            RuntimePlayers.IsValidIndex(VoterIndex) ? RuntimePlayers[VoterIndex].Name : TEXT("AI"),
            TEXT("MCP vote failed or returned an invalid target, so I abstain."),
            RuntimePlayers.IsValidIndex(VoterIndex) ? RuntimePlayers[VoterIndex].AvatarColor : FLinearColor(0.80f, 0.82f, 0.86f, 1.0f));
        RuntimeActiveAIPlayerIndex = INDEX_NONE;
        RuntimeAIRequestType = EAIWerewolfRuntimeAIRequest::None;
        bRuntimeAIRequestInFlight = false;
        StartNextRuntimeVoteAI();
        return;
    }

    if (RuntimePlayers.IsValidIndex(TargetIndex))
    {
        RuntimeVoteCounts.FindOrAdd(TargetIndex)++;
        AppendRuntimeChatMessage(
            RuntimePlayers.IsValidIndex(VoterIndex) ? RuntimePlayers[VoterIndex].Name : TEXT("AI"),
            FString::Printf(TEXT("MCP vote: I vote for %s."), *RuntimePlayers[TargetIndex].Name),
            RuntimePlayers.IsValidIndex(VoterIndex) ? RuntimePlayers[VoterIndex].AvatarColor : FLinearColor(0.80f, 0.82f, 0.86f, 1.0f));
    }

    RuntimeActiveAIPlayerIndex = INDEX_NONE;
    RuntimeAIRequestType = EAIWerewolfRuntimeAIRequest::None;
    bRuntimeAIRequestInFlight = false;
    StartNextRuntimeVoteAI();
}

void AWinyunqDemoGameMode::FinishRuntimeVoteResolution()
{
    int32 HighestVotes = 0;
    TArray<int32> TopTargets;
    for (const TPair<int32, int32>& Pair : RuntimeVoteCounts)
    {
        if (Pair.Value > HighestVotes)
        {
            HighestVotes = Pair.Value;
            TopTargets.Reset();
            TopTargets.Add(Pair.Key);
        }
        else if (Pair.Value == HighestVotes)
        {
            TopTargets.Add(Pair.Key);
        }
    }

    if (TopTargets.Num() == 0)
    {
        RuntimePhase = EAIWerewolfRuntimePhase::Results;
        AppendRuntimeChatMessage(TEXT("System / 系统"), TEXT("No valid votes were cast."), FLinearColor(0.80f, 0.82f, 0.86f, 1.0f));
        RefreshRuntimePhaseText();
        return;
    }

    const int32 EliminatedIndex = TopTargets[FMath::RandRange(0, TopTargets.Num() - 1)];
    if (RuntimePlayers.IsValidIndex(EliminatedIndex))
    {
        RuntimePlayers[EliminatedIndex].bAlive = false;
        RuntimePhase = EAIWerewolfRuntimePhase::Results;
        AppendRuntimeChatMessage(
            TEXT("System / 系统"),
            FString::Printf(TEXT("%s was voted out with %d votes. Role: %s."), *RuntimePlayers[EliminatedIndex].Name, HighestVotes, *GetRuntimeRoleDisplay(RuntimePlayers[EliminatedIndex])),
            FLinearColor(0.90f, 0.40f, 0.36f, 1.0f));
    }

    HumanVoteTarget = INDEX_NONE;
    RuntimeVoteCounts.Reset();
    RefreshRuntimePlayers();
    RebuildRuntimeVoteButtons();
    if (!EvaluateRuntimeWinCondition())
    {
        RefreshRuntimePhaseText();
    }
}

bool AWinyunqDemoGameMode::EvaluateRuntimeWinCondition()
{
    int32 AliveWerewolves = 0;
    int32 AliveGoodPlayers = 0;
    for (const FAIWerewolfRuntimePlayer& Player : RuntimePlayers)
    {
        if (!Player.bAlive)
        {
            continue;
        }

        if (Player.IsWerewolf())
        {
            ++AliveWerewolves;
        }
        else
        {
            ++AliveGoodPlayers;
        }
    }

    if (AliveWerewolves <= 0)
    {
        RuntimePhase = EAIWerewolfRuntimePhase::Ended;
        AppendRuntimeChatMessage(TEXT("System / 系统"), TEXT("Villagers win. All werewolves have been eliminated."), FLinearColor(0.35f, 0.82f, 0.48f, 1.0f));
        RefreshRuntimePlayers();
        RebuildRuntimeVoteButtons();
        RefreshRuntimePhaseText();
        return true;
    }

    if (AliveWerewolves >= AliveGoodPlayers)
    {
        RuntimePhase = EAIWerewolfRuntimePhase::Ended;
        AppendRuntimeChatMessage(TEXT("System / 系统"), TEXT("Werewolves win. They now control the village."), FLinearColor(0.90f, 0.32f, 0.32f, 1.0f));
        RefreshRuntimePlayers();
        RebuildRuntimeVoteButtons();
        RefreshRuntimePhaseText();
        return true;
    }

    return false;
}

bool AWinyunqDemoGameMode::StartRuntimeAIRequest(EAIWerewolfRuntimeAIRequest RequestType, int32 ActorPlayerIndex, const FString& UserPrompt)
{
    if (!ULiteRtLmBlueprintLibrary::IsLiteRtLmModelLoaded())
    {
        bRuntimeAIRequestInFlight = false;
        RuntimeAIRequestType = EAIWerewolfRuntimeAIRequest::None;
        RuntimeActiveAIPlayerIndex = INDEX_NONE;
        StopRuntimeAIWatchdog();
        RefreshRuntimePhaseText();
        return false;
    }

    if (bRuntimeAIRequestInFlight)
    {
        return true;
    }

    if (!RuntimePlayers.IsValidIndex(ActorPlayerIndex) || !RuntimePlayers[ActorPlayerIndex].bAlive)
    {
        return false;
    }

    RuntimeAIRequestType = RequestType;
    RuntimeActiveAIPlayerIndex = ActorPlayerIndex;
    RuntimeActiveAIResponse.Reset();
    RuntimeAIRequestStartTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : FPlatformTime::Seconds();
    RuntimeActiveAIRequestSerial = ++RuntimeAIRequestSerial;
    RuntimeAIReceivedChunkCount = 0;
    RuntimeAIReceivedCharCount = 0;
    bRuntimeAIStopRequested = false;
    bRuntimeAIStallWarningShown = false;
    bRuntimeAIRequestInFlight = true;

    const FString RequestLabel = GetRuntimeAIRequestLabel(RequestType);
    const FString ActorName = RuntimePlayers.IsValidIndex(ActorPlayerIndex)
        ? RuntimePlayers[ActorPlayerIndex].Name
        : FString::Printf(TEXT("P%d"), ActorPlayerIndex + 1);
    UE_LOG(LogTemp, Warning, TEXT("[AIWerewolf] AI request #%d started: type=%s actor=P%d %s prompt_len=%d language=%s"),
        RuntimeActiveAIRequestSerial,
        *RequestLabel,
        ActorPlayerIndex + 1,
        *ActorName,
        UserPrompt.Len(),
        *GetRuntimeLanguageLabel());
    AppendRuntimeChatMessage(
        TEXT("System / 系统"),
        FString::Printf(TEXT("LiteRT-LM request #%d started: %s for P%d %s."),
            RuntimeActiveAIRequestSerial,
            *RequestLabel,
            ActorPlayerIndex + 1,
            *ActorName),
        FLinearColor(0.55f, 0.70f, 0.95f, 1.0f));
    RefreshRuntimePhaseText();
    RefreshRuntimePlayers();

    const FString SystemPrompt = FString::Printf(
        TEXT("You are a local LiteRT-LM agent inside an AI Werewolf demo. ")
        TEXT("Act only as the assigned player and use that player's private role. ")
        TEXT("The same model is reused for many players, so obey the current prompt over previous turns. ")
        TEXT("Keep responses concise. Do not reveal hidden roles unless it is your own strategic choice. ")
        TEXT("%s ")
        TEXT("When a night target or vote is requested, use the MCP tool werewolf_vote with target like P3 and a short reason. ")
        TEXT("If tool calls are unavailable, output TARGET=P# and REASON=short reason."),
        *GetRuntimeLanguagePromptInstruction());

    FLiteRtLmBlueprintChunkDelegate ChunkDelegate;
    ChunkDelegate.BindDynamic(this, &AWinyunqDemoGameMode::HandleRuntimeAIChunk);

    FLiteRtLmBlueprintDoneDelegate DoneDelegate;
    DoneDelegate.BindDynamic(this, &AWinyunqDemoGameMode::HandleRuntimeAIDone);

    const FLiteRtLmSamplingParams SamplingParams = ULiteRtLmBlueprintLibrary::MakeLiteRtLmSamplingParams(
        0.7f,
        0.9f,
        40,
        160,
        ELiteRtLmConstraintType::None,
        TEXT(""));

    UObject* SessionOwner = RuntimeAISessionOwners.IsValidIndex(ActorPlayerIndex) && IsValid(RuntimeAISessionOwners[ActorPlayerIndex])
        ? RuntimeAISessionOwners[ActorPlayerIndex].Get()
        : static_cast<UObject*>(this);

    ULiteRtLmBlueprintLibrary::SendLiteRtLmJsonChat(
        this,
        BuildRuntimeAIMessageJson(SystemPrompt, UserPrompt),
        BuildRuntimeWerewolfToolsJson(),
        SessionOwner,
        ChunkDelegate,
        DoneDelegate,
        SamplingParams);

    StartRuntimeAIWatchdog();
    return true;
}

void AWinyunqDemoGameMode::StartRuntimeAIWatchdog()
{
    StopRuntimeAIWatchdog();
    if (!GetWorld())
    {
        return;
    }

    GetWorldTimerManager().SetTimer(
        RuntimeAIWatchdogTimerHandle,
        this,
        &AWinyunqDemoGameMode::HandleRuntimeAIWatchdogTick,
        RuntimeAIWatchdogIntervalSeconds,
        true,
        RuntimeAIWatchdogIntervalSeconds);
}

void AWinyunqDemoGameMode::StopRuntimeAIWatchdog()
{
    if (GetWorld())
    {
        GetWorldTimerManager().ClearTimer(RuntimeAIWatchdogTimerHandle);
    }
}

void AWinyunqDemoGameMode::HandleRuntimeAIWatchdogTick()
{
    if (!bRuntimeAIRequestInFlight || RuntimeAIRequestType == EAIWerewolfRuntimeAIRequest::None)
    {
        StopRuntimeAIWatchdog();
        return;
    }

    const double NowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : FPlatformTime::Seconds();
    const double ElapsedSeconds = FMath::Max(0.0, NowSeconds - RuntimeAIRequestStartTimeSeconds);
    const FString RequestLabel = GetRuntimeAIRequestLabel(RuntimeAIRequestType);
    const FString ActorName = RuntimePlayers.IsValidIndex(RuntimeActiveAIPlayerIndex)
        ? RuntimePlayers[RuntimeActiveAIPlayerIndex].Name
        : FString::Printf(TEXT("P%d"), RuntimeActiveAIPlayerIndex + 1);

    UE_LOG(LogTemp, Warning, TEXT("[AIWerewolf] AI request #%d pending: type=%s actor=P%d %s elapsed=%.1fs chunks=%d chars=%d stop_requested=%d"),
        RuntimeActiveAIRequestSerial,
        *RequestLabel,
        RuntimeActiveAIPlayerIndex + 1,
        *ActorName,
        ElapsedSeconds,
        RuntimeAIReceivedChunkCount,
        RuntimeAIReceivedCharCount,
        bRuntimeAIStopRequested ? 1 : 0);
    RefreshRuntimePhaseText();
    RefreshRuntimePlayers();

    if (!bRuntimeAIStallWarningShown && ElapsedSeconds >= RuntimeAIStallWarningSeconds && RuntimeAIReceivedChunkCount == 0)
    {
        bRuntimeAIStallWarningShown = true;
        AppendRuntimeChatMessage(
            TEXT("System / 系统"),
            FString::Printf(TEXT("LiteRT-LM request #%d has waited %.0fs with no token yet. Check logcat/Output Log for [AIWerewolf]."),
                RuntimeActiveAIRequestSerial,
                ElapsedSeconds),
            FLinearColor(0.93f, 0.72f, 0.30f, 1.0f));
        RefreshRuntimePhaseText();
    }

    if (ElapsedSeconds >= RuntimeAIRequestTimeoutSeconds)
    {
        FinishRuntimeAIRequestFromTimeout();
    }
}

void AWinyunqDemoGameMode::FinishRuntimeAIRequestFromTimeout()
{
    if (!bRuntimeAIRequestInFlight || RuntimeAIRequestType == EAIWerewolfRuntimeAIRequest::None)
    {
        StopRuntimeAIWatchdog();
        return;
    }

    const EAIWerewolfRuntimeAIRequest TimedOutRequestType = RuntimeAIRequestType;
    const int32 TimedOutPlayerIndex = RuntimeActiveAIPlayerIndex;
    const double NowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : FPlatformTime::Seconds();
    const double ElapsedSeconds = FMath::Max(0.0, NowSeconds - RuntimeAIRequestStartTimeSeconds);
    const FString RequestLabel = GetRuntimeAIRequestLabel(TimedOutRequestType);
    const FString ActorName = RuntimePlayers.IsValidIndex(TimedOutPlayerIndex)
        ? RuntimePlayers[TimedOutPlayerIndex].Name
        : FString::Printf(TEXT("P%d"), TimedOutPlayerIndex + 1);

    bRuntimeAIStopRequested = true;
    StopRuntimeAIWatchdog();

    UE_LOG(LogTemp, Error, TEXT("[AIWerewolf] AI request #%d timeout: type=%s actor=P%d %s elapsed=%.1fs chunks=%d chars=%d; calling StopLiteRtLmInference"),
        RuntimeActiveAIRequestSerial,
        *RequestLabel,
        TimedOutPlayerIndex + 1,
        *ActorName,
        ElapsedSeconds,
        RuntimeAIReceivedChunkCount,
        RuntimeAIReceivedCharCount);
    AppendRuntimeChatMessage(
        TEXT("System / 系统"),
        FString::Printf(TEXT("LiteRT-LM request #%d timed out after %.0fs (%s, P%d %s). The demo will cancel it and continue."),
            RuntimeActiveAIRequestSerial,
            ElapsedSeconds,
            *RequestLabel,
            TimedOutPlayerIndex + 1,
            *ActorName),
        FLinearColor(0.93f, 0.42f, 0.36f, 1.0f));

    ULiteRtLmBlueprintLibrary::StopLiteRtLmInference();
    RuntimeActiveAIResponse.Reset();

    switch (TimedOutRequestType)
    {
    case EAIWerewolfRuntimeAIRequest::NightKill:
        bRuntimeAIRequestInFlight = false;
        RuntimeAIRequestType = EAIWerewolfRuntimeAIRequest::None;
        CompleteRuntimeNightPhaseFromAI(TEXT(""));
        break;
    case EAIWerewolfRuntimeAIRequest::DiscussionSpeech:
        CompleteRuntimeDiscussionAI(TEXT("[LiteRT-LM timed out before responding.]"));
        break;
    case EAIWerewolfRuntimeAIRequest::Vote:
        CompleteRuntimeVoteAI(TEXT(""));
        break;
    default:
        bRuntimeAIRequestInFlight = false;
        RuntimeAIRequestType = EAIWerewolfRuntimeAIRequest::None;
        RuntimeActiveAIPlayerIndex = INDEX_NONE;
        RefreshRuntimePhaseText();
        RefreshRuntimePlayers();
        break;
    }
}

FString AWinyunqDemoGameMode::GetRuntimeAIRequestLabel(EAIWerewolfRuntimeAIRequest RequestType) const
{
    switch (RequestType)
    {
    case EAIWerewolfRuntimeAIRequest::NightKill:
        return TEXT("NightKill");
    case EAIWerewolfRuntimeAIRequest::DiscussionSpeech:
        return TEXT("DiscussionSpeech");
    case EAIWerewolfRuntimeAIRequest::Vote:
        return TEXT("Vote");
    default:
        return TEXT("None");
    }
}

FLiteRtLmConfig AWinyunqDemoGameMode::BuildRuntimeModelConfig(const FString& ModelPath) const
{
    FLiteRtLmConfig Config;
    Config.ModelPath = ModelPath;
    Config.ToolsJson = BuildRuntimeWerewolfToolsJson();
    Config.bEnableVision = false;
    Config.bEnableAudio = false;
    Config.bEnableStreaming = true;

    Config.Backend = TEXT("cpu");
    Config.MaxNumTokens = 1024;
    Config.NumThreads = 4;
    Config.PrefillChunkSize = 512;
    Config.bOptimizeShader = false;
    Config.bShareConstantTensors = true;
    Config.bEnableHostMappedPointer = false;

    return Config;
}

FString AWinyunqDemoGameMode::BuildRuntimeWerewolfToolsJson() const
{
    return TEXT("[{\"type\":\"function\",\"function\":{\"name\":\"werewolf_vote\",\"description\":\"MCP voting channel for the AI Werewolf demo. Cast one night target or daytime vote.\",\"parameters\":{\"type\":\"object\",\"properties\":{\"target\":{\"type\":\"string\",\"description\":\"Player id such as P2 or P7.\"},\"reason\":{\"type\":\"string\",\"description\":\"Short reason for the vote.\"}},\"required\":[\"target\"]}}}]");
}

FString AWinyunqDemoGameMode::BuildRuntimeAIMessageJson(const FString& SystemPrompt, const FString& UserPrompt) const
{
    TArray<TSharedPtr<FJsonValue>> Messages;

    TSharedPtr<FJsonObject> SystemMessage = MakeShared<FJsonObject>();
    SystemMessage->SetStringField(TEXT("role"), TEXT("system"));
    SystemMessage->SetStringField(TEXT("content"), SystemPrompt);
    Messages.Add(MakeShared<FJsonValueObject>(SystemMessage));

    TSharedPtr<FJsonObject> UserMessage = MakeShared<FJsonObject>();
    UserMessage->SetStringField(TEXT("role"), TEXT("user"));
    UserMessage->SetStringField(TEXT("content"), UserPrompt);
    Messages.Add(MakeShared<FJsonValueObject>(UserMessage));

    FString MessagesJson;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&MessagesJson);
    FJsonSerializer::Serialize(Messages, Writer);
    return MessagesJson;
}

FString AWinyunqDemoGameMode::BuildRuntimeRecentTranscriptText(int32 MaxLines) const
{
    if (RuntimeTranscript.Num() == 0)
    {
        return TEXT("Recent table transcript: none yet.\n");
    }

    FString Transcript = TEXT("Recent table transcript:\n");
    const int32 StartIndex = FMath::Max(0, RuntimeTranscript.Num() - FMath::Max(1, MaxLines));
    for (int32 Index = StartIndex; Index < RuntimeTranscript.Num(); ++Index)
    {
        Transcript += FString::Printf(TEXT("- %s\n"), *RuntimeTranscript[Index]);
    }
    return Transcript;
}

FString AWinyunqDemoGameMode::BuildRuntimeGameStateText(int32 PerspectivePlayerIndex) const
{
    FString State = FString::Printf(TEXT("Round=%d\nPlayers:\n"), RuntimeRoundIndex);
    for (int32 PlayerIndex = 0; PlayerIndex < RuntimePlayers.Num(); ++PlayerIndex)
    {
        const FAIWerewolfRuntimePlayer& Player = RuntimePlayers[PlayerIndex];
        FString PrivateNote = TEXT("");
        if (PlayerIndex == PerspectivePlayerIndex)
        {
            PrivateNote = FString::Printf(TEXT(", you_are=%s"), *Player.Role);
        }
        else if (RuntimePlayers.IsValidIndex(PerspectivePlayerIndex)
            && RuntimePlayers[PerspectivePlayerIndex].IsWerewolf()
            && Player.IsWerewolf())
        {
            PrivateNote = TEXT(", known_werewolf_teammate=true");
        }

        State += FString::Printf(
            TEXT("- P%d %s: %s, public_role=%s%s\n"),
            PlayerIndex + 1,
            *Player.Name,
            Player.bAlive ? TEXT("alive") : TEXT("out"),
            *GetRuntimeRoleDisplay(Player),
            *PrivateNote);
    }
    State += BuildRuntimeRecentTranscriptText();
    return State;
}

FString AWinyunqDemoGameMode::BuildRuntimeNightPrompt(int32 ActorPlayerIndex) const
{
    FString Candidates;
    for (int32 PlayerIndex = 0; PlayerIndex < RuntimePlayers.Num(); ++PlayerIndex)
    {
        if (RuntimePlayers[PlayerIndex].bAlive && !RuntimePlayers[PlayerIndex].IsWerewolf())
        {
            Candidates += FString::Printf(TEXT("P%d "), PlayerIndex + 1);
        }
    }

    return FString::Printf(
        TEXT("%s\n%s\nYou are %s. Your private role is %s. The werewolf team must choose one living non-werewolf night target from: %s.\nUse the MCP function werewolf_vote with target=P# and a short reason. If tools are unavailable, output:\nTARGET=P#\nREASON=short reason"),
        *BuildRuntimeGameStateText(ActorPlayerIndex),
        *GetRuntimeLanguagePromptInstruction(),
        RuntimePlayers.IsValidIndex(ActorPlayerIndex) ? *RuntimePlayers[ActorPlayerIndex].Name : TEXT("the werewolf team"),
        RuntimePlayers.IsValidIndex(ActorPlayerIndex) ? *RuntimePlayers[ActorPlayerIndex].Role : TEXT("Werewolf"),
        *Candidates);
}

FString AWinyunqDemoGameMode::BuildRuntimeDiscussionPrompt(int32 ActorPlayerIndex) const
{
    if (!RuntimePlayers.IsValidIndex(ActorPlayerIndex))
    {
        return BuildRuntimeGameStateText(ActorPlayerIndex);
    }

    return FString::Printf(
        TEXT("%s\n%s\nCurrent turn: P%d %s.\nYou are %s. Your private role is %s. Give one concise daytime speech as this player in first person. Do not output JSON. Do not call tools. Do not choose a final vote yet."),
        *BuildRuntimeGameStateText(ActorPlayerIndex),
        *GetRuntimeLanguagePromptInstruction(),
        ActorPlayerIndex + 1,
        *RuntimePlayers[ActorPlayerIndex].Name,
        *RuntimePlayers[ActorPlayerIndex].Name,
        *RuntimePlayers[ActorPlayerIndex].Role);
}

FString AWinyunqDemoGameMode::BuildRuntimeVotePrompt(int32 ActorPlayerIndex) const
{
    if (!RuntimePlayers.IsValidIndex(ActorPlayerIndex))
    {
        return BuildRuntimeGameStateText(ActorPlayerIndex);
    }

    FString Candidates;
    for (int32 PlayerIndex = 0; PlayerIndex < RuntimePlayers.Num(); ++PlayerIndex)
    {
        if (IsRuntimeVoteTargetValid(PlayerIndex, ActorPlayerIndex))
        {
            Candidates += FString::Printf(TEXT("P%d "), PlayerIndex + 1);
        }
    }

    return FString::Printf(
        TEXT("%s\n%s\nYou are %s. Your private role is %s. Choose one vote target from: %s.\nUse the MCP function werewolf_vote with target=P# and a short reason. If tools are unavailable, output:\nTARGET=P#\nREASON=short reason"),
        *BuildRuntimeGameStateText(ActorPlayerIndex),
        *GetRuntimeLanguagePromptInstruction(),
        *RuntimePlayers[ActorPlayerIndex].Name,
        *RuntimePlayers[ActorPlayerIndex].Role,
        *Candidates);
}

int32 AWinyunqDemoGameMode::ParseRuntimeTargetIndexFromText(const FString& Text) const
{
    FString UpperText = Text.ToUpper();
    UpperText.ReplaceInline(TEXT(" "), TEXT(""));

    int32 TargetPos = UpperText.Find(TEXT("TARGET=P"));
    if (TargetPos != INDEX_NONE)
    {
        const FString NumberText = UpperText.Mid(TargetPos + 8, 2);
        const int32 PlayerNumber = FCString::Atoi(*NumberText);
        if (PlayerNumber >= 1 && PlayerNumber <= RuntimePlayers.Num())
        {
            return PlayerNumber - 1;
        }
    }

    for (int32 PlayerIndex = 0; PlayerIndex < RuntimePlayers.Num(); ++PlayerIndex)
    {
        const FString Token = FString::Printf(TEXT("P%d"), PlayerIndex + 1);
        if (UpperText.Contains(Token))
        {
            return PlayerIndex;
        }
    }

    return INDEX_NONE;
}

bool AWinyunqDemoGameMode::IsRuntimeVoteTargetValid(int32 TargetIndex, int32 VoterIndex) const
{
    return RuntimePlayers.IsValidIndex(TargetIndex)
        && RuntimePlayers.IsValidIndex(VoterIndex)
        && TargetIndex != VoterIndex
        && RuntimePlayers[TargetIndex].bAlive;
}

void AWinyunqDemoGameMode::HandleRuntimeVoteClicked(int32 PlayerIndex)
{
    if (!RuntimePlayers.IsValidIndex(PlayerIndex))
    {
        return;
    }

    if (RuntimePhase != EAIWerewolfRuntimePhase::Voting)
    {
        AppendRuntimeChatMessage(
            TEXT("System / 系统"),
            FString::Printf(TEXT("%s selected. Voting opens after discussion."), *RuntimePlayers[PlayerIndex].Name),
            FLinearColor(0.80f, 0.82f, 0.86f, 1.0f));
        return;
    }

    if (!RuntimePlayers[PlayerIndex].bAlive)
    {
        AppendRuntimeChatMessage(TEXT("System / 系统"), TEXT("You cannot vote for an eliminated player."), FLinearColor(0.93f, 0.72f, 0.30f, 1.0f));
        return;
    }

    if (PlayerIndex == RuntimeHumanPlayerIndex)
    {
        AppendRuntimeChatMessage(TEXT("System / 系统"), TEXT("You cannot vote for yourself."), FLinearColor(0.93f, 0.72f, 0.30f, 1.0f));
        return;
    }

    HumanVoteTarget = PlayerIndex;
    AppendRuntimeChatMessage(
        TEXT("You / 玩家"),
        FString::Printf(TEXT("I vote for %s."), *RuntimePlayers[PlayerIndex].Name),
        RuntimePlayers.IsValidIndex(RuntimeHumanPlayerIndex) ? RuntimePlayers[RuntimeHumanPlayerIndex].AvatarColor : FLinearColor(0.22f, 0.48f, 0.95f, 1.0f));
    RefreshRuntimePlayers();
    RebuildRuntimeVoteButtons();
}

void AWinyunqDemoGameMode::RebuildRuntimeVoteButtons()
{
    if (!RuntimeVoteBox)
    {
        return;
    }

    RuntimeVoteBox->ClearChildren();

    if (RuntimePhase != EAIWerewolfRuntimePhase::Voting)
    {
        UTextBlock* VoteHint = CreateRuntimeText(
            TEXT("RuntimeVoteHint"),
            TEXT("Vote controls appear during the voting phase. / 投票阶段会显示投票按钮。"),
            14,
            FLinearColor(0.62f, 0.68f, 0.76f, 1.0f),
            true);
        RuntimeVoteBox->AddChildToVerticalBox(VoteHint);
        return;
    }

    UTextBlock* VoteTitle = CreateRuntimeText(
        TEXT("RuntimeVoteTitle"),
        TEXT("Vote Target / 投票目标"),
        16,
        FLinearColor(0.92f, 0.92f, 0.95f, 1.0f),
        true);
    RuntimeVoteBox->AddChildToVerticalBox(VoteTitle);

    UHorizontalBox* VoteRows[2] =
    {
        ConstructRuntimeWidget<UHorizontalBox>(ActiveAIWerewolfWidget, TEXT("RuntimeVoteRowA")),
        ConstructRuntimeWidget<UHorizontalBox>(ActiveAIWerewolfWidget, TEXT("RuntimeVoteRowB"))
    };
    RuntimeVoteBox->AddChildToVerticalBox(VoteRows[0]);
    RuntimeVoteBox->AddChildToVerticalBox(VoteRows[1]);

    static const FName VoteHandlers[MaxRuntimePlayers] =
    {
        GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleRuntimeVotePlayer1Clicked),
        GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleRuntimeVotePlayer2Clicked),
        GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleRuntimeVotePlayer3Clicked),
        GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleRuntimeVotePlayer4Clicked),
        GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleRuntimeVotePlayer5Clicked),
        GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleRuntimeVotePlayer6Clicked),
        GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleRuntimeVotePlayer7Clicked),
        GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleRuntimeVotePlayer8Clicked),
        GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleRuntimeVotePlayer9Clicked),
        GET_FUNCTION_NAME_CHECKED(AWinyunqDemoGameMode, HandleRuntimeVotePlayer10Clicked)
    };

    for (int32 PlayerIndex = 0; PlayerIndex < RuntimePlayers.Num(); ++PlayerIndex)
    {
        if (!RuntimePlayers[PlayerIndex].bAlive || RuntimePlayers[PlayerIndex].bHuman)
        {
            continue;
        }

        const FString Label = PlayerIndex == HumanVoteTarget
            ? FString::Printf(TEXT("%s *"), *RuntimePlayers[PlayerIndex].Name)
            : RuntimePlayers[PlayerIndex].Name;

        UButton* VoteButton = CreateRuntimeButton(
            FName(*FString::Printf(TEXT("RuntimeVoteButton_%d"), PlayerIndex + 1)),
            Label,
            VoteHandlers[PlayerIndex],
            PlayerIndex == HumanVoteTarget ? FLinearColor(0.72f, 0.47f, 0.18f, 1.0f) : FLinearColor(0.18f, 0.24f, 0.34f, 1.0f));
        if (UHorizontalBoxSlot* VoteSlot = VoteRows[PlayerIndex % 2]->AddChildToHorizontalBox(VoteButton))
        {
            VoteSlot->SetPadding(FMargin(0.0f, 6.0f, 8.0f, 0.0f));
        }
    }
}

void AWinyunqDemoGameMode::RefreshRuntimePlayers()
{
    for (int32 PlayerIndex = 0; PlayerIndex < MaxRuntimePlayers; ++PlayerIndex)
    {
        const bool bValidPlayer = RuntimePlayers.IsValidIndex(PlayerIndex);
        if (RuntimePlayerButtons.IsValidIndex(PlayerIndex) && RuntimePlayerButtons[PlayerIndex])
        {
            RuntimePlayerButtons[PlayerIndex]->SetVisibility(bValidPlayer ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
            RuntimePlayerButtons[PlayerIndex]->SetIsEnabled(bValidPlayer);
        }

        if (!bValidPlayer)
        {
            continue;
        }

        const FAIWerewolfRuntimePlayer& Player = RuntimePlayers[PlayerIndex];
        if (RuntimePlayerAvatarTexts.IsValidIndex(PlayerIndex) && RuntimePlayerAvatarTexts[PlayerIndex])
        {
            FString AvatarLabel = TEXT("OUT");
            if (Player.bAlive)
            {
                AvatarLabel = Player.bHuman
                    ? TEXT("YOU")
                    : FString::Printf(TEXT("MCP\nAI %02d"), PlayerIndex + 1);
            }
            RuntimePlayerAvatarTexts[PlayerIndex]->SetText(FText::FromString(AvatarLabel));
        }
        if (RuntimePlayerNameTexts.IsValidIndex(PlayerIndex) && RuntimePlayerNameTexts[PlayerIndex])
        {
            RuntimePlayerNameTexts[PlayerIndex]->SetText(FText::FromString(Player.Name));
        }
        if (RuntimePlayerRoleTexts.IsValidIndex(PlayerIndex) && RuntimePlayerRoleTexts[PlayerIndex])
        {
            RuntimePlayerRoleTexts[PlayerIndex]->SetText(FText::FromString(GetRuntimeRoleDisplay(Player)));
        }
        if (RuntimePlayerStatusTexts.IsValidIndex(PlayerIndex) && RuntimePlayerStatusTexts[PlayerIndex])
        {
            FString Status = Player.bAlive ? TEXT("Alive / 存活") : TEXT("Out / 出局");
            if (Player.bAlive && PlayerIndex == RuntimeActiveAIPlayerIndex)
            {
                if (bRuntimeAIRequestInFlight && !RuntimePlayers[PlayerIndex].bHuman)
                {
                    const double NowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : FPlatformTime::Seconds();
                    const double ElapsedSeconds = FMath::Max(0.0, NowSeconds - RuntimeAIRequestStartTimeSeconds);
                    Status = FString::Printf(TEXT("Thinking %.0fs / 思考 %.0f 秒"), ElapsedSeconds, ElapsedSeconds);
                }
                else
                {
                    Status = RuntimePlayers[PlayerIndex].bHuman ? TEXT("Your turn / 你的回合") : TEXT("Thinking / 思考中");
                }
            }
            if (RuntimePhase == EAIWerewolfRuntimePhase::Voting && Player.bAlive && !Player.bHuman && PlayerIndex != RuntimeActiveAIPlayerIndex)
            {
                Status = PlayerIndex == HumanVoteTarget ? TEXT("Voted / 已选择") : TEXT("Tap to vote / 点击投票");
            }
            RuntimePlayerStatusTexts[PlayerIndex]->SetText(FText::FromString(Status));
            RuntimePlayerStatusTexts[PlayerIndex]->SetColorAndOpacity(FSlateColor(Player.bAlive ? FLinearColor(0.76f, 0.90f, 0.78f, 1.0f) : FLinearColor(0.92f, 0.46f, 0.42f, 1.0f)));
        }
    }
}

void AWinyunqDemoGameMode::RefreshRuntimePhaseText()
{
    FString PhaseLabel;
    FString Instruction;
    FString NextLabel;

    switch (RuntimePhase)
    {
    case EAIWerewolfRuntimePhase::Night:
        PhaseLabel = FString::Printf(TEXT("Night %d / 第 %d 夜"), RuntimeRoundIndex, RuntimeRoundIndex);
        Instruction = TEXT("Night is resolving automatically for this demo. / 本 Demo 自动结算夜晚。");
        NextLabel = TEXT("Day / 天亮");
        break;
    case EAIWerewolfRuntimePhase::Discussion:
        PhaseLabel = FString::Printf(TEXT("Day %d Discussion / 第 %d 天发言"), RuntimeRoundIndex, RuntimeRoundIndex);
        Instruction = TEXT("Read AI speeches, type your speech, then start voting. / 阅读 AI 发言，输入你的发言，然后开始投票。");
        NextLabel = TEXT("Start Vote / 开始投票");
        break;
    case EAIWerewolfRuntimePhase::Voting:
        PhaseLabel = FString::Printf(TEXT("Day %d Voting / 第 %d 天投票"), RuntimeRoundIndex, RuntimeRoundIndex);
        Instruction = TEXT("Tap a living AI player, then resolve the vote. / 点击存活 AI 玩家，然后结算投票。");
        NextLabel = TEXT("Resolve Vote / 结算投票");
        break;
    case EAIWerewolfRuntimePhase::Results:
        PhaseLabel = FString::Printf(TEXT("Day %d Result / 第 %d 天结果"), RuntimeRoundIndex, RuntimeRoundIndex);
        Instruction = TEXT("Review the result, then continue to the next night. / 查看结果后进入下一夜。");
        NextLabel = TEXT("Next Night / 下一夜");
        break;
    case EAIWerewolfRuntimePhase::Ended:
        PhaseLabel = TEXT("Game Over / 游戏结束");
        Instruction = TEXT("Roles are revealed. Tap New Game to play again. / 身份已揭示，点击新局重新开始。");
        NextLabel = TEXT("New Game / 新局");
        break;
    default:
        PhaseLabel = TEXT("Setup / 设置");
        Instruction = TEXT("Choose player count and start the game. / 选择人数并开始游戏。");
        NextLabel = TEXT("Start / 开始");
        break;
    }

    if (bRuntimeWaitingForHumanSpeech)
    {
        Instruction = TEXT("Your speech turn is blocking progress. Type your statement and tap Speak. / 轮到你发言，输入后点击发言。");
        NextLabel = TEXT("Your Turn / 你的回合");
    }
    else if (bRuntimeAIRequestInFlight || RuntimePendingAIPlayers.Num() > 0)
    {
        if (bRuntimeAIRequestInFlight)
        {
            const double NowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : FPlatformTime::Seconds();
            const double ElapsedSeconds = FMath::Max(0.0, NowSeconds - RuntimeAIRequestStartTimeSeconds);
            Instruction = FString::Printf(TEXT("AI request #%d is running %.0fs. Filter logs with [AIWerewolf]. / AI 请求 #%d 已运行 %.0f 秒，日志过滤 [AIWerewolf]。"),
                RuntimeActiveAIRequestSerial,
                ElapsedSeconds,
                RuntimeActiveAIRequestSerial,
                ElapsedSeconds);
        }
        else
        {
            Instruction = TEXT("AI turns are queued and will run automatically. / AI 回合已排队，将自动执行。");
        }
        NextLabel = TEXT("AI Thinking... / AI 思考中");
    }

    if (RuntimePhaseText)
    {
        RuntimePhaseText->SetText(FText::FromString(PhaseLabel));
    }
    if (RuntimeInstructionText)
    {
        RuntimeInstructionText->SetText(FText::FromString(Instruction));
    }
    if (RuntimeNextPhaseButtonText)
    {
        RuntimeNextPhaseButtonText->SetText(FText::FromString(NextLabel));
    }
    if (RuntimeNextPhaseButton)
    {
        RuntimeNextPhaseButton->SetIsEnabled(!bRuntimeAIRequestInFlight && RuntimePendingAIPlayers.Num() == 0 && !bRuntimeWaitingForHumanSpeech);
    }
}

void AWinyunqDemoGameMode::AppendRuntimeChatMessage(const FString& Speaker, const FString& Message, const FLinearColor& AccentColor)
{
    if (!RuntimeChatScrollBox)
    {
        return;
    }

    const int32 MessageIndex = RuntimeChatScrollBox->GetChildrenCount();
    UBorder* RowBorder = ConstructRuntimeWidget<UBorder>(
        ActiveAIWerewolfWidget,
        FName(*FString::Printf(TEXT("RuntimeChatRow_%d"), MessageIndex)));
    RowBorder->SetBrushColor(FLinearColor(0.085f, 0.10f, 0.125f, 1.0f));
    RowBorder->SetPadding(FMargin(8.0f, 6.0f));

    UHorizontalBox* RowBox = ConstructRuntimeWidget<UHorizontalBox>(
        ActiveAIWerewolfWidget,
        FName(*FString::Printf(TEXT("RuntimeChatRowBox_%d"), MessageIndex)));
    RowBorder->SetContent(RowBox);

    UTextBlock* SpeakerText = CreateRuntimeText(
        FName(*FString::Printf(TEXT("RuntimeChatSpeaker_%d"), MessageIndex)),
        Speaker,
        15,
        AccentColor,
        true);
    if (UHorizontalBoxSlot* SpeakerSlot = RowBox->AddChildToHorizontalBox(SpeakerText))
    {
        SpeakerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        SpeakerSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
        SpeakerSlot->SetVerticalAlignment(VAlign_Top);
    }

    UTextBlock* MessageText = CreateRuntimeText(
        FName(*FString::Printf(TEXT("RuntimeChatMessage_%d"), MessageIndex)),
        Message,
        15,
        FLinearColor(0.93f, 0.94f, 0.96f, 1.0f),
        true);
    if (UHorizontalBoxSlot* MessageSlot = RowBox->AddChildToHorizontalBox(MessageText))
    {
        MessageSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(RuntimeChatScrollBox->AddChild(RowBorder)))
    {
        RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
    }
    RuntimeChatScrollBox->ScrollToEnd();

    FString TranscriptLine = FString::Printf(TEXT("%s: %s"), *Speaker, *Message);
    TranscriptLine.ReplaceInline(TEXT("\r"), TEXT(" "));
    TranscriptLine.ReplaceInline(TEXT("\n"), TEXT(" "));
    RuntimeTranscript.Add(TranscriptLine.Left(360));
    if (RuntimeTranscript.Num() > 48)
    {
        RuntimeTranscript.RemoveAt(0, RuntimeTranscript.Num() - 48);
    }
}

FString AWinyunqDemoGameMode::BuildRuntimeAISpeech(int32 PlayerIndex) const
{
    if (!RuntimePlayers.IsValidIndex(PlayerIndex))
    {
        return TEXT("I need more information before voting.");
    }

    const int32 TargetIndex = ChooseRuntimeVoteTarget(PlayerIndex);
    const FString TargetName = RuntimePlayers.IsValidIndex(TargetIndex) ? RuntimePlayers[TargetIndex].Name : TEXT("someone quiet");
    const FString& PlayerRole = RuntimePlayers[PlayerIndex].Role;

    if (PlayerRole == TEXT("Werewolf"))
    {
        return FString::Printf(TEXT("%s feels suspicious to me. The night kill may be a frame, but we need pressure there."), *TargetName);
    }
    if (PlayerRole == TEXT("Seer"))
    {
        return FString::Printf(TEXT("I want to hear more from %s. Voting patterns will tell us more than random claims."), *TargetName);
    }
    if (PlayerRole == TEXT("Witch"))
    {
        return FString::Printf(TEXT("I am watching who pushes too fast. %s should explain their logic before voting."), *TargetName);
    }
    if (PlayerRole == TEXT("Guard"))
    {
        return FString::Printf(TEXT("Do not rush. I prefer a vote on %s unless a better clue appears."), *TargetName);
    }

    return FString::Printf(TEXT("I am a villager. %s has been avoiding clear statements, so I want pressure there."), *TargetName);
}

FString AWinyunqDemoGameMode::GetRuntimeRoleDisplay(const FAIWerewolfRuntimePlayer& Player) const
{
    const bool bRevealRole = Player.bHuman || !Player.bAlive || RuntimePhase == EAIWerewolfRuntimePhase::Ended;
    if (!bRevealRole)
    {
        return TEXT("Hidden / 身份隐藏");
    }

    if (Player.Role == TEXT("Werewolf"))
    {
        return TEXT("Werewolf / 狼人");
    }
    if (Player.Role == TEXT("Seer"))
    {
        return TEXT("Seer / 预言家");
    }
    if (Player.Role == TEXT("Witch"))
    {
        return TEXT("Witch / 女巫");
    }
    if (Player.Role == TEXT("Guard"))
    {
        return TEXT("Guard / 守卫");
    }
    return TEXT("Villager / 村民");
}

int32 AWinyunqDemoGameMode::ChooseRuntimeNightTarget() const
{
    TArray<int32> Candidates;
    for (int32 PlayerIndex = 0; PlayerIndex < RuntimePlayers.Num(); ++PlayerIndex)
    {
        if (RuntimePlayers[PlayerIndex].bAlive && !RuntimePlayers[PlayerIndex].IsWerewolf())
        {
            Candidates.Add(PlayerIndex);
        }
    }

    return Candidates.Num() > 0 ? Candidates[FMath::RandRange(0, Candidates.Num() - 1)] : INDEX_NONE;
}

int32 AWinyunqDemoGameMode::ChooseRuntimeVoteTarget(int32 VoterIndex) const
{
    if (!RuntimePlayers.IsValidIndex(VoterIndex))
    {
        return INDEX_NONE;
    }

    TArray<int32> Candidates;
    for (int32 PlayerIndex = 0; PlayerIndex < RuntimePlayers.Num(); ++PlayerIndex)
    {
        if (PlayerIndex == VoterIndex || !RuntimePlayers[PlayerIndex].bAlive)
        {
            continue;
        }

        if (RuntimePlayers[VoterIndex].IsWerewolf() && RuntimePlayers[PlayerIndex].IsWerewolf())
        {
            continue;
        }

        Candidates.Add(PlayerIndex);
    }

    return Candidates.Num() > 0 ? Candidates[FMath::RandRange(0, Candidates.Num() - 1)] : INDEX_NONE;
}

TArray<int32> AWinyunqDemoGameMode::GetAliveRuntimePlayerIndices() const
{
    TArray<int32> AliveIndices;
    for (int32 PlayerIndex = 0; PlayerIndex < RuntimePlayers.Num(); ++PlayerIndex)
    {
        if (RuntimePlayers[PlayerIndex].bAlive)
        {
            AliveIndices.Add(PlayerIndex);
        }
    }
    return AliveIndices;
}

void AWinyunqDemoGameMode::HandleRuntimeSendClicked()
{
    if (!RuntimePlayerInput)
    {
        return;
    }

    FString Message = RuntimePlayerInput->GetText().ToString().TrimStartAndEnd();
    if (Message.IsEmpty())
    {
        return;
    }

    RuntimePlayerInput->SetText(FText::GetEmpty());
    if (RuntimePhase != EAIWerewolfRuntimePhase::Discussion)
    {
        AppendRuntimeChatMessage(TEXT("You / 玩家"), Message, RuntimePlayers.IsValidIndex(RuntimeHumanPlayerIndex) ? RuntimePlayers[RuntimeHumanPlayerIndex].AvatarColor : FLinearColor(0.22f, 0.48f, 0.95f, 1.0f));
        return;
    }

    if (!bRuntimeWaitingForHumanSpeech || RuntimeActiveAIPlayerIndex != RuntimeHumanPlayerIndex)
    {
        AppendRuntimeChatMessage(
            TEXT("System / 系统"),
            TEXT("Please wait for your speech turn. / 请等待轮到你发言。"),
            FLinearColor(0.93f, 0.72f, 0.30f, 1.0f));
        return;
    }

    AppendRuntimeChatMessage(TEXT("You / 玩家"), Message, RuntimePlayers.IsValidIndex(RuntimeHumanPlayerIndex) ? RuntimePlayers[RuntimeHumanPlayerIndex].AvatarColor : FLinearColor(0.22f, 0.48f, 0.95f, 1.0f));
    bRuntimeWaitingForHumanSpeech = false;
    RuntimeActiveAIPlayerIndex = INDEX_NONE;
    ++RuntimeDiscussionTurnCursor;
    StartNextRuntimeDiscussionTurn();
}

void AWinyunqDemoGameMode::HandleRuntimeNextPhaseClicked()
{
    AdvanceRuntimeWerewolfPhase();
}

void AWinyunqDemoGameMode::HandleRuntimeResetClicked()
{
    ResetRuntimeWerewolfGame();
}

void AWinyunqDemoGameMode::HandleRuntimeBackSetupClicked()
{
    ReturnToSetupUI();
}

void AWinyunqDemoGameMode::HandleRuntimeVotePlayer1Clicked()
{
    HandleRuntimeVoteClicked(0);
}

void AWinyunqDemoGameMode::HandleRuntimeVotePlayer2Clicked()
{
    HandleRuntimeVoteClicked(1);
}

void AWinyunqDemoGameMode::HandleRuntimeVotePlayer3Clicked()
{
    HandleRuntimeVoteClicked(2);
}

void AWinyunqDemoGameMode::HandleRuntimeVotePlayer4Clicked()
{
    HandleRuntimeVoteClicked(3);
}

void AWinyunqDemoGameMode::HandleRuntimeVotePlayer5Clicked()
{
    HandleRuntimeVoteClicked(4);
}

void AWinyunqDemoGameMode::HandleRuntimeVotePlayer6Clicked()
{
    HandleRuntimeVoteClicked(5);
}

void AWinyunqDemoGameMode::HandleRuntimeVotePlayer7Clicked()
{
    HandleRuntimeVoteClicked(6);
}

void AWinyunqDemoGameMode::HandleRuntimeVotePlayer8Clicked()
{
    HandleRuntimeVoteClicked(7);
}

void AWinyunqDemoGameMode::HandleRuntimeVotePlayer9Clicked()
{
    HandleRuntimeVoteClicked(8);
}

void AWinyunqDemoGameMode::HandleRuntimeVotePlayer10Clicked()
{
    HandleRuntimeVoteClicked(9);
}

void AWinyunqDemoGameMode::HandleRuntimeAIChunk(const FString& TextChunk)
{
    if (!bRuntimeAIRequestInFlight || RuntimeAIRequestType == EAIWerewolfRuntimeAIRequest::None)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AIWerewolf] Ignoring late AI chunk len=%d because no request is in flight."), TextChunk.Len());
        return;
    }

    ++RuntimeAIReceivedChunkCount;
    RuntimeAIReceivedCharCount += TextChunk.Len();
    RuntimeActiveAIResponse += TextChunk;

    if (RuntimeAIReceivedChunkCount == 1)
    {
        const double NowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : FPlatformTime::Seconds();
        UE_LOG(LogTemp, Warning, TEXT("[AIWerewolf] AI request #%d first chunk after %.1fs: len=%d"),
            RuntimeActiveAIRequestSerial,
            FMath::Max(0.0, NowSeconds - RuntimeAIRequestStartTimeSeconds),
            TextChunk.Len());
        AppendRuntimeChatMessage(
            TEXT("System / 系统"),
            FString::Printf(TEXT("LiteRT-LM request #%d received first token."), RuntimeActiveAIRequestSerial),
            FLinearColor(0.55f, 0.70f, 0.95f, 1.0f));
    }
}

void AWinyunqDemoGameMode::HandleRuntimeAIDone(const FLiteRtLmResult& Result)
{
    if (!bRuntimeAIRequestInFlight || RuntimeAIRequestType == EAIWerewolfRuntimeAIRequest::None)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AIWerewolf] Ignoring late AI done callback. text_len=%d json_len=%d error=%s"),
            Result.FullText.Len(),
            Result.FullJson.Len(),
            *Result.ErrorMsg);
        return;
    }

    StopRuntimeAIWatchdog();
    const EAIWerewolfRuntimeAIRequest CompletedRequestType = RuntimeAIRequestType;
    const int32 CompletedPlayerIndex = RuntimeActiveAIPlayerIndex;
    const double NowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : FPlatformTime::Seconds();
    const double ElapsedSeconds = FMath::Max(0.0, NowSeconds - RuntimeAIRequestStartTimeSeconds);
    FString AIText = !Result.FullText.IsEmpty() ? Result.FullText : RuntimeActiveAIResponse;
    UE_LOG(LogTemp, Warning, TEXT("[AIWerewolf] AI request #%d done: type=%s actor=P%d elapsed=%.1fs chunks=%d chars=%d full_text=%d full_json=%d error=%s"),
        RuntimeActiveAIRequestSerial,
        *GetRuntimeAIRequestLabel(CompletedRequestType),
        CompletedPlayerIndex + 1,
        ElapsedSeconds,
        RuntimeAIReceivedChunkCount,
        RuntimeAIReceivedCharCount,
        Result.FullText.Len(),
        Result.FullJson.Len(),
        *Result.ErrorMsg);

    if ((CompletedRequestType == EAIWerewolfRuntimeAIRequest::NightKill || CompletedRequestType == EAIWerewolfRuntimeAIRequest::Vote)
        && !Result.FullJson.IsEmpty())
    {
        AIText += TEXT("\n");
        AIText += Result.FullJson;
    }

    if (!Result.ErrorMsg.IsEmpty())
    {
        AppendRuntimeChatMessage(
            TEXT("System / 系统"),
            FString::Printf(TEXT("LiteRT-LM AI request failed. Error: %s"), *Result.ErrorMsg),
            FLinearColor(0.93f, 0.72f, 0.30f, 1.0f));
        AIText.Reset();
    }

    RuntimeActiveAIResponse.Reset();

    switch (CompletedRequestType)
    {
    case EAIWerewolfRuntimeAIRequest::NightKill:
        bRuntimeAIRequestInFlight = false;
        RuntimeAIRequestType = EAIWerewolfRuntimeAIRequest::None;
        CompleteRuntimeNightPhaseFromAI(AIText);
        break;
    case EAIWerewolfRuntimeAIRequest::DiscussionSpeech:
        CompleteRuntimeDiscussionAI(AIText);
        break;
    case EAIWerewolfRuntimeAIRequest::Vote:
        CompleteRuntimeVoteAI(AIText);
        break;
    default:
        bRuntimeAIRequestInFlight = false;
        RuntimeAIRequestType = EAIWerewolfRuntimeAIRequest::None;
        RuntimeActiveAIPlayerIndex = INDEX_NONE;
        RefreshRuntimePhaseText();
        break;
    }
}
