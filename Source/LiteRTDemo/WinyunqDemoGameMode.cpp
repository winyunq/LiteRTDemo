#include "WinyunqDemoGameMode.h"
#include "DemoTavernHUD.h"
#include "WinyunqMcpWrapper.h"
#include "Blueprint/UserWidget.h"

AWinyunqDemoGameMode::AWinyunqDemoGameMode()
{
    HUDClass = ADemoTavernHUD::StaticClass();
}

void AWinyunqDemoGameMode::BeginPlay()
{
    Super::BeginPlay();

    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        UWinyunqMcpWrapper* ChatUI = CreateWidget<UWinyunqMcpWrapper>(PC, UWinyunqMcpWrapper::StaticClass());
        if (ChatUI)
        {
            ChatUI->AddToViewport();
            
            // Show mouse cursor
            PC->bShowMouseCursor = true;
            PC->SetInputMode(FInputModeGameAndUI());
        }
    }
}
