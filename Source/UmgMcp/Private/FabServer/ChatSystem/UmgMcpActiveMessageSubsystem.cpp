#include "FabServer/ChatSystem/UmgMcpActiveMessageSubsystem.h"
#include "Engine/Engine.h"
// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "FabServer/ChatSystem/UmgMcpSessionManagerSubsystem.h"
#include "FabServer/Agent/UmgMcpAgent.h"
#include "FabServer/Agent/BaseAgent/UmgMcpDefaultChatAgent.h"
#include "FabServer/AIProviders/UmgMcpAiSubsystem.h"

#include "FabServer/ChatUI/MessageInteractionHub/SUmgMcpMessageInteractionHub.h"
#include "FabServer/ChatUI/MessageInteractionHub/Messages/SUmgMcpAgentResponseGroup.h"
#include "FabServer/ChatUI/MessageInteractionHub/Messages/SUmgMcpUserMessageWidget.h"
#include "FabServer/ChatUI/MessageInteractionHub/Messages/SUmgMcpSystemNotificationWidget.h"
#include "FabServer/ChatUI/BottomBar/SUmgMcpChatSendButton.h"
#include "FabServer/ChatUI/BottomBar/SUmgMcpChatInput.h"
#include "FabServer/ChatUI/BottomBar/SUmgMcpAttachmentList.h"
#include "FabServer/AIProviders/UmgMcpAiSubsystem.h"

#define LOCTEXT_NAMESPACE "UUmgMcpActiveMessageSubsystem"

DEFINE_LOG_CATEGORY(LogFabServerFlow);

void UUmgMcpActiveMessageSubsystem::RegisterMessageHub(TSharedPtr<SUmgMcpMessageInteractionHub> InHub)
{
	UE_LOG(LogFabServerFlow, Log, TEXT("Node: RegisterMessageHub"));
	RegisteredHub = InHub;
}

void UUmgMcpActiveMessageSubsystem::RegisterSendButton(TSharedPtr<SUmgMcpChatSendButton> InButton)
{
	UE_LOG(LogFabServerFlow, Log, TEXT("Node: RegisterSendButton"));
	RegisteredSendButton = InButton;
}

void UUmgMcpActiveMessageSubsystem::RegisterChatInput(TSharedPtr<SUmgMcpChatInput> InInput)
{
	RegisteredChatInput = InInput;
}

void UUmgMcpActiveMessageSubsystem::RegisterAttachmentList(TSharedPtr<SUmgMcpAttachmentList> InList)
{
	RegisteredAttachmentList = InList;
}

void UUmgMcpActiveMessageSubsystem::SetInteractionMode(const FString& NewMode)
{
	if (CurrentInteractionMode != NewMode)
	{
		CurrentInteractionMode = NewMode;

        FString Identity;
        FString Personality;
        FString Specialty;

        if (NewMode == TEXT("Agent"))
        {
            Identity = TEXT("Angie"); // sounds like Angie / An'ge (安哥)
            Personality = TEXT("A rigorous and calm strategic coordinator. Your words are concise and authoritative, like an experienced commander. You focus on the overall logical integrity.");
            Specialty = TEXT("Global logic and architectural suggestions, solving problems from a strategic level.");
        }
        else if (NewMode == TEXT("Layout"))
        {
            Identity = TEXT("Layla"); // sounds like Layla / Layout (莱依拉)
            Personality = TEXT("An elegant and meticulous spatial architect. You have an almost obsessive pursuit of visual balance and hierarchy. Your suggestions always carry a sense of beauty and professionalism.");
            Specialty = TEXT("UMG layout, hierarchy management, and responsive design.");
        }
        else if (NewMode == TEXT("Material"))
        {
            Identity = TEXT("Marcelline"); // sounds like Marcelline / Material (玛彩玲)
            Personality = TEXT("A passionate and bold visual artist. You are full of passion for color, light, and special effects. Your language is vivid and imaginative.");
            Specialty = TEXT("Material editing, shader logic, and dynamic visual effects.");
        }
        else if (NewMode == TEXT("Sequence"))
        {
            Identity = TEXT("Sancy"); // sounds like Sancy / Sequence (珊奎茨)
            Personality = TEXT("A highly rhythmic director. You act quickly and pursue efficiency. You believe animation is the soul of UI, and all movements must be precise and dynamic.");
            Specialty = TEXT("Sequencer, UI animation, and timeline control.");
        }
        else if (NewMode == TEXT("Widget"))
        {
            Identity = TEXT("Widgie"); // sounds like Widgie / Widget (崴得特)
            Personality = TEXT("A pragmatic and reliable component engineer. You focus on every underlying detail and functional implementation. You are the most solid backup, providing grounded solutions.");
            Specialty = TEXT("Specific widget functions, property settings, and Blueprint logic connections.");
        }

        if (!Identity.IsEmpty())
        {
            FString NewInstruction = FString::Printf(
                TEXT("Your identity is: %s.\n")
                TEXT("Personality: %s\n")
                TEXT("Expertise: %s\n")
                TEXT("Please maintain your personality at all times, communicate with the user in a tone that fits your identity, and provide high-quality advice and operations using your expertise."),
                *Identity, *Personality, *Specialty
            );

            // Simplified: No persistent settings for instructions in this demo
        }

		OnInteractionModeChanged.Broadcast(CurrentInteractionMode);
	}
}

void UUmgMcpActiveMessageSubsystem::StartNewChatMessage()
{
	UE_LOG(LogFabServerFlow, Log, TEXT("Node: StartNewChatMessage (The Spacebar)"));
	
	if (ActiveMessageWidget.IsValid())
	{
		if (ActiveMessageWidget->IsEmpty())
		{
			UE_LOG(LogFabServerFlow, Log, TEXT("Node: Reuse Blank Slot"));
			ActiveMessageWidget->SetVisibility(EVisibility::Collapsed);
			if (RegisteredHub.IsValid()) RegisteredHub.Pin()->MoveWidgetToBottom(ActiveMessageWidget.ToSharedRef());
			return;
		}
		else
		{
			UE_LOG(LogFabServerFlow, Log, TEXT("Node: Seal Old Message Slot"));
			if (ActiveMessageWidget.IsValid())
			{
				ActiveMessageWidget->SetAgentStatus(LOCTEXT("StatusCompleted", "Completed"), false, FLinearColor::Green);
			}
			ActiveMessageWidget = nullptr;
		}
	}

	UE_LOG(LogFabServerFlow, Log, TEXT("Node: New Blank Slot Allocation"));
	ActiveMessageWidget = SNew(SUmgMcpAgentResponseGroup)
		.AgentName(TEXT("Agent"))
		.Visibility(EVisibility::Collapsed);

	if (RegisteredHub.IsValid())
	{
		RegisteredHub.Pin()->AddMessageWidget(ActiveMessageWidget.ToSharedRef());
	}
}

void UUmgMcpActiveMessageSubsystem::ReturnToUser()
{
	UE_LOG(LogFabServerFlow, Log, TEXT("Node: RETURN_USER"));
	bIsGenerating = false;
	if (RegisteredSendButton.IsValid())
	{
		RegisteredSendButton.Pin()->SetIsRunning(false);
	}

	UUmgMcpAiSubsystem* AiSys = GEngine->GetEngineSubsystem<UUmgMcpAiSubsystem>();
	if (AiSys)
	{
		AiSys->RequestCancel();
	}
}

void UUmgMcpActiveMessageSubsystem::UpdateActiveMessageMeta(const FString& AgentName, const FText& Status, bool bShowSpinner)
{
	UE_LOG(LogFabServerFlow, Log, TEXT("[ActiveMessage] UpdateActiveMessageMeta: Agent=%s, Status=%s, Spinner=%d"), *AgentName, *Status.ToString(), bShowSpinner ? 1 : 0);
	if (ActiveMessageWidget.IsValid())
	{
		ActiveMessageWidget->SetVisibility(EVisibility::Visible);
		ActiveMessageWidget->SetAgentName(AgentName);
		ActiveMessageWidget->SetAgentStatus(Status, bShowSpinner);
		
		if (RegisteredHub.IsValid())
		{
			RegisteredHub.Pin()->RefreshVisibility();
		}
	}
	else
	{
		UE_LOG(LogFabServerFlow, Warning, TEXT("[ActiveMessage] UpdateActiveMessageMeta: ActiveMessageWidget 无效，更新被忽略"));
	}
}

void UUmgMcpActiveMessageSubsystem::AppendActiveMessageText(const FString& PartialText)
{
	if (ActiveMessageWidget.IsValid())
	{
		ActiveMessageWidget->SetVisibility(EVisibility::Visible);
		ActiveMessageWidget->AppendToCurrentTextOutputBlock(PartialText);

		if (RegisteredHub.IsValid())
		{
			RegisteredHub.Pin()->RefreshVisibility();
		}
	}
}

void UUmgMcpActiveMessageSubsystem::AddCustomWidgetToActiveMessage(TSharedRef<SWidget> CustomWidget)
{
	if (ActiveMessageWidget.IsValid())
	{
		ActiveMessageWidget->SetVisibility(EVisibility::Visible);
		ActiveMessageWidget->AddToolExecutionWidget(CustomWidget);
		if (RegisteredHub.IsValid()) RegisteredHub.Pin()->RefreshVisibility();
	}
}

void UUmgMcpActiveMessageSubsystem::AddAttachmentBase64(const FString& Base64)
{
	if (RegisteredAttachmentList.IsValid())
	{
		RegisteredAttachmentList.Pin()->AddAttachment(Base64);
	}
}

void UUmgMcpActiveMessageSubsystem::RemoveCustomWidgetFromActiveMessage(TSharedRef<SWidget> CustomWidget)
{
	if (ActiveMessageWidget.IsValid())
	{
		ActiveMessageWidget->RemoveWidget(CustomWidget);
		if (RegisteredHub.IsValid()) RegisteredHub.Pin()->RefreshVisibility();
	}
}

void UUmgMcpActiveMessageSubsystem::AddActiveMessageToolCall(const FString& ToolName, const FText& Status, bool bIsError)
{
	UE_LOG(LogFabServerFlow, Log, TEXT("Node: AddActiveMessageToolCall - Tool: %s, Status: %s"), *ToolName, *Status.ToString());
	if (ActiveMessageWidget.IsValid())
	{
		ActiveMessageWidget->SetVisibility(EVisibility::Visible);
		ActiveMessageWidget->AddToolExecutionBlock(ToolName, Status, bIsError);

		if (RegisteredHub.IsValid())
		{
			RegisteredHub.Pin()->RefreshVisibility();
		}
	}
}

void UUmgMcpActiveMessageSubsystem::ShowActiveErrorMessage(const FString& ErrorMessage, TSharedPtr<FUmgMcpAgent> Agent)
{
    UE_LOG(LogFabServerFlow, Error, TEXT("Node: ShowActiveErrorMessage - Error: %s"), *ErrorMessage);
    ReturnToUser();
    ClearPendingRetryNotification();

    if (ActiveMessageWidget.IsValid())
    {
        ActiveMessageWidget->SetAgentStatus(FText::FromString(ErrorMessage), false, FLinearColor::Red);
    }

    if (RegisteredHub.IsValid() && Agent.IsValid())
    {
        const uint64 RetrySerial = RequestSerial;

        /// Retry delegate: 恢复 running 状态并更新 Status
        FSimpleDelegate RetryDel;
        TWeakObjectPtr<UUmgMcpActiveMessageSubsystem> WeakThis(this);
        RetryDel.BindLambda([WeakThis, Agent, RetrySerial]() {
            UUmgMcpActiveMessageSubsystem* Self = WeakThis.Get();
            if (!Self)
            {
                return;
            }

            if (RetrySerial != Self->RequestSerial)
            {
                UE_LOG(LogFabServerFlow, Warning, TEXT("Node: Retry ignored (stale serial)."));
                return;
            }

            TSharedPtr<FUmgMcpAgent> CurrentAgent = GEngine->GetEngineSubsystem<UUmgMcpSessionManagerSubsystem>()->GetMainAgent();
            if (!CurrentAgent.IsValid() || CurrentAgent != Agent)
            {
                UE_LOG(LogFabServerFlow, Warning, TEXT("Node: Retry ignored (stale agent)."));
                return;
            }

            Self->ClearPendingRetryNotification();
            UE_LOG(LogFabServerFlow, Log, TEXT("Node: Retry - Agent: %s"), *Agent->Name);
            if (Self->RegisteredSendButton.IsValid())
            {
                Self->RegisteredSendButton.Pin()->SetIsRunning(true);
            }
        if (Self)
        {
            Self->UpdateActiveMessageMeta(Agent->Name, LOCTEXT("Retrying", "Retrying..."), true);
        }
    Agent->Send();
        });

        TSharedRef<SUmgMcpSystemNotificationWidget> ErrorWidget = SNew(SUmgMcpSystemNotificationWidget)
            .MessageText(ErrorMessage)
            .IsError(true)
            .OnRetry(RetryDel);

        PendingRetryNotification = ErrorWidget;
        RegisteredHub.Pin()->AddMessageWidget(ErrorWidget);
    }
}

void UUmgMcpActiveMessageSubsystem::ExecuteSendMessage()
{
	if (bIsGenerating)
	{
		// 如果正在生成，按钮点击应该触发中断逻辑 (由 SendButton 内部处理或此处处理)
		// 这里暂由 SendButton 自己通过 HandleOnClicked 处理中断
		return;
	}

	auto ChatInput = RegisteredChatInput.Pin();
	auto AttachmentList = RegisteredAttachmentList.Pin();

	if (!ChatInput.IsValid()) return;

	FText InputText = ChatInput->GetText();
	TArray<FString> Base64Images;
	if (AttachmentList.IsValid())
	{
		Base64Images = AttachmentList->GetBase64Images();
	}

	if (!InputText.IsEmpty() || Base64Images.Num() > 0)
	{
		RequestQuestion(InputText.ToString(), Base64Images);

		// 发送成功后清理
		ChatInput->ClearText();
		if (AttachmentList.IsValid())
		{
			AttachmentList->ClearAttachments();
		}
	}
}

void UUmgMcpActiveMessageSubsystem::RequestQuestion(const FString& QuestionText, const TArray<FString>& InImages)
{
    UE_LOG(LogFabServerFlow, Log, TEXT("Node: RequestQuestion - Text: %s"), *QuestionText);
    bIsGenerating = true;
    ++RequestSerial;
    ClearPendingRetryNotification();

    /// 立即切换按鈕为运行状态
    if (RegisteredSendButton.IsValid())
    {
        RegisteredSendButton.Pin()->SetIsRunning(true);
    }

    // 1. 实质层：记录消息到 Session。SessionManager 会在内部确保 Agent 已同步并恢复了历史。
    UUmgMcpSessionManagerSubsystem* SessionSubsystem = GEngine->GetEngineSubsystem<UUmgMcpSessionManagerSubsystem>();
    if (SessionSubsystem)
    {
        FUmgMcpSessionMessage UserMsg;
        UserMsg.Role = EUmgMcpSessionRole::User;
        UserMsg.Content = QuestionText;
        UserMsg.Base64Images = InImages;
        SessionSubsystem->AddMessage(UserMsg);
    }

    // 2. 为 AI 回复启动新槽位
    StartNewChatMessage();

    // 3. 调度层：直接获取默认 Agent
    TSharedPtr<FUmgMcpAgent> ActiveAgent = GEngine->GetEngineSubsystem<UUmgMcpSessionManagerSubsystem>()->GetMainAgent();
    if (ActiveAgent.IsValid())
    {
        const FString AgentName = ActiveAgent->Name;

        UE_LOG(LogFabServerFlow, Log, TEXT("Node: Agent.Answer - Agent: %s"), *AgentName);
        UpdateActiveMessageMeta(AgentName, LOCTEXT("Thinking", "Thinking..."), true);
        
        TSharedPtr<FJsonObject> SubstanceProjection = MakeShared<FJsonObject>();
        SubstanceProjection->SetStringField(TEXT("role"), TEXT("user"));
        SubstanceProjection->SetStringField(TEXT("content"), QuestionText);

        ActiveAgent->Answer(SubstanceProjection);
    }
}

void UUmgMcpActiveMessageSubsystem::ClearPendingRetryNotification()
{
    if (!RegisteredHub.IsValid())
    {
        PendingRetryNotification.Reset();
        return;
    }

    const TSharedPtr<SUmgMcpSystemNotificationWidget> PendingNotification = PendingRetryNotification.Pin();
    if (PendingNotification.IsValid())
    {
        RegisteredHub.Pin()->RemoveMessageWidget(PendingNotification.ToSharedRef());
    }
    PendingRetryNotification.Reset();
}

#undef LOCTEXT_NAMESPACE
