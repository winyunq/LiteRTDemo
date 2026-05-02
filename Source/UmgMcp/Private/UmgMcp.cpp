#include "UmgMcp.h"
#include "Engine/Engine.h"
#if WITH_EDITOR
#include "PropertyEditorModule.h"
#include "Subsystems/AssetEditorSubsystem.h"
#endif

#include "FabServer/ChatUI/SUmgMcpChatWindow.h"
#include "UmgMcpStyle.h"
#include "Internationalization/Culture.h"
#if WITH_EDITOR
#include "ToolMenus.h"
#endif
#include "Widgets/Docking/SDockTab.h"
// #include "FileManage/UmgAttentionSubsystem.h" // Deleted
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#if WITH_EDITOR
#include "WorkflowOrientedApp/ApplicationMode.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"
#include "WorkflowOrientedApp/WorkflowCentricApplication.h" 
#endif
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FUmgMcpModule"

DEFINE_LOG_CATEGORY(LogUmgMcp);

static const FName UmgMcpTabName("UmgMcpChat");

void FUmgMcpModule::StartupModule()
{
    FUmgMcpStyle::Initialize();
    FUmgMcpStyle::ReloadTextures();
    
    PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddRaw(this, &FUmgMcpModule::Initialize);
}

void FUmgMcpModule::ShutdownModule()
{
    FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);

#if WITH_EDITOR
#endif
    
    FUmgMcpStyle::Shutdown();
}

void FUmgMcpModule::Initialize()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(UmgMcpTabName, FOnSpawnTab::CreateLambda([](const FSpawnTabArgs& Args)
    {
        return SNew(SDockTab)
            .TabRole(ETabRole::NomadTab)
            .OnCanCloseTab_Lambda([]() 
            {
#if WITH_EDITOR
                const FText Msg = LOCTEXT("UmgMcpConfirmClose", "We will save your session state and release all model cache files (GPU memory) for you to carry out other project activities. \n\nAre you sure you want to close the window?");
                const FText Title = LOCTEXT("UmgMcpConfirmCloseTitle", "Confirm Close");
                EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo, Msg, Title);
                return (Result == EAppReturnType::Yes);
#else
                return true;
#endif
            })
            [
                SNew(SUmgMcpChatWindow)
            ];
    }))
    .SetDisplayName(LOCTEXT("UmgMcpChatTabTitle", "UMG AI Assistant"))
    .SetTooltipText(LOCTEXT("UmgMcpChatTooltipText", "Open UMG AI Assistant"))

    .SetIcon(FSlateIcon(FUmgMcpStyle::GetStyleSetName(), "UmgMcp.PluginIcon"));


}

void FUmgMcpModule::RegisterAssetEditorFactory(UObject* Asset, TSharedPtr<FUmgMcpChatTabFactory> Factory) {}

TSharedRef<SDockTab> FUmgMcpModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs) { return SNew(SDockTab); }

void FUmgMcpModule::RegisterMenus() {}

void FUmgMcpModule::RegisterAssetEditorMenu(IAssetEditorInstance* Instance)
{
}

void FUmgMcpModule::OnAssetEditorOpened(UObject* Asset)
{
}

IMPLEMENT_MODULE(FUmgMcpModule, UmgMcp)

#undef LOCTEXT_NAMESPACE
