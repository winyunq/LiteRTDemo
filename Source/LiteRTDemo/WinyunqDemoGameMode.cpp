#include "WinyunqDemoGameMode.h"
#include "DemoTavernHUD.h"
#include "WinyunqMcpWrapper.h"
#include "Blueprint/UserWidget.h"
#include "UObject/ConstructorHelpers.h"

AWinyunqDemoGameMode::AWinyunqDemoGameMode()
{
    HUDClass = ADemoTavernHUD::StaticClass();

    static ConstructorHelpers::FClassFinder<UUserWidget> AIWerewolfWidgetFinder(
        TEXT("/Game/AIWerewolf/WBP_AIWerewolfGame"));
    if (AIWerewolfWidgetFinder.Succeeded())
    {
        AIWerewolfWidgetClass = AIWerewolfWidgetFinder.Class;
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
            MainUI->AddToViewport();
            
            // Show mouse cursor
            PC->bShowMouseCursor = true;
            PC->SetInputMode(FInputModeGameAndUI());
        }
    }
}
