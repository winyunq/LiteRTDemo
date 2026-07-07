// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "FabServer/ChatSystem/UmgMcpActiveMessageSubsystem.h"
#include "FabServer/ChatSystem/UmgMcpSessionManagerSubsystem.h"
#include "FabServer/Agent/UmgMcpAgent.h"
#include "FabServer/Agent/BaseAgent/UmgMcpDefaultChatAgent.h"
#include "FabServer/AIProviders/UmgMcpAiSubsystem.h"

#include "FabServer/ChatUI/MessageInteractionHub/SUmgMcpMessageInteractionHub.h"
#include "FabServer/ChatUI/MessageInteractionHub/Messages/SUmgMcpAgentResponseGroup.h"
#include "FabServer/ChatUI/MessageInteractionHub/Messages/SUmgMcpSystemNotificationWidget.h"
#include "FabServer/ChatUI/BottomBar/SUmgMcpChatSendButton.h"
#include "FabServer/ChatUI/BottomBar/SUmgMcpChatInput.h"
#include "FabServer/ChatUI/BottomBar/SUmgMcpAttachmentList.h"
#include "FabServer/AIProviders/UmgMcpAiSubsystem.h"
#include "Engine/Engine.h"

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

	// 通知 AI 子系统中断当前推理（推理引擎立即停止 Generate 循环）
	if (UUmgMcpAiSubsystem* AiSys = GEngine ? GEngine->GetEngineSubsystem<UUmgMcpAiSubsystem>() : nullptr)
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

            TSharedPtr<FUmgMcpAgent> CurrentAgent = GEngine && GEngine->GetEngineSubsystem<UUmgMcpSessionManagerSubsystem>() ?
                                                    GEngine->GetEngineSubsystem<UUmgMcpSessionManagerSubsystem>()->GetMainAgent() : nullptr;
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
    UUmgMcpSessionManagerSubsystem* SessionSubsystem = GEngine ? GEngine->GetEngineSubsystem<UUmgMcpSessionManagerSubsystem>() : nullptr;
    if (SessionSubsystem)
    {
        FUmgMcpSessionMessage UserMsg;
        UserMsg.Role = EUmgMcpSessionRole::User;
        UserMsg.Content = QuestionText;
        UserMsg.Base64Images = InImages;
        SessionSubsystem->AddMessage(UserMsg);
    }

    // 2. 表现层：启动 UI 槽位
    StartNewChatMessage();

    // 3. 调度层：获取默认 Agent
    TSharedPtr<FUmgMcpAgent> ActiveAgent = GEngine && GEngine->GetEngineSubsystem<UUmgMcpSessionManagerSubsystem>() ?
                                           GEngine->GetEngineSubsystem<UUmgMcpSessionManagerSubsystem>()->GetMainAgent() : nullptr;
    if (ActiveAgent.IsValid())
    {
        const FString AgentName = ActiveAgent->Name;

        UE_LOG(LogFabServerFlow, Log, TEXT("Node: Agent.Answer - Agent: %s"), *AgentName);
        UpdateActiveMessageMeta(AgentName, LOCTEXT("Thinking", "Thinking..."), true);
        
        TSharedPtr<FJsonObject> SubstanceProjection = MakeShared<FJsonObject>();
        SubstanceProjection->SetStringField(TEXT("role"), TEXT("user"));

        if (InImages.Num() == 0)
        {
            SubstanceProjection->SetStringField(TEXT("content"), QuestionText);
        }
        else
        {
            TArray<TSharedPtr<FJsonValue>> ContentArray;

            // Text part
            TSharedPtr<FJsonObject> TextPart = MakeShared<FJsonObject>();
            TextPart->SetStringField(TEXT("type"), TEXT("text"));
            TextPart->SetStringField(TEXT("text"), QuestionText);
            ContentArray.Add(MakeShared<FJsonValueObject>(TextPart));

            // Image part (Base64 URL)
            for (const FString& Base64Image : InImages)
            {
                TSharedPtr<FJsonObject> ImagePart = MakeShared<FJsonObject>();
                ImagePart->SetStringField(TEXT("type"), TEXT("image_url"));

                TSharedPtr<FJsonObject> ImageUrlObj = MakeShared<FJsonObject>();
                FString FinalUrl = Base64Image.StartsWith(TEXT("data:image")) ? Base64Image :
                                  FString::Printf(TEXT("data:image/jpeg;base64,%s"), *Base64Image);
                ImageUrlObj->SetStringField(TEXT("url"), FinalUrl);

                ImagePart->SetObjectField(TEXT("image_url"), ImageUrlObj);
                ContentArray.Add(MakeShared<FJsonValueObject>(ImagePart));
            }
            SubstanceProjection->SetArrayField(TEXT("content"), ContentArray);
        }

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
