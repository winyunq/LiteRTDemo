#include "FabServer/Agent/UmgMcpAgent.h"
#include "Engine/Engine.h"
// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "FabServer/ChatSystem/UmgMcpActiveMessageSubsystem.h"
#include "FabServer/AIProviders/UmgMcpAiSubsystem.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Dom/JsonObject.h"
#include "Styling/SlateStyle.h"
#include "Brushes/SlateImageBrush.h"
#include "FabServer/ChatSystem/UmgMcpSessionManagerSubsystem.h"
#include "Misc/Guid.h"

#define LOCTEXT_NAMESPACE "FUmgMcpAgent"

FUmgMcpAgent::FUmgMcpAgent(const FString& InRelativeJsonPath)
{
	UE_LOG(LogFabServerFlow, Log, TEXT("[Agent] 构造: 加载配置 %s"), *InRelativeJsonPath);
	auto Plugin = IPluginManager::Get().FindPlugin(TEXT("FabUmgMcp"));
	FString PluginRootDir = Plugin.IsValid() ? Plugin->GetBaseDir() : TEXT("");
	FString FullConfigPath = PluginRootDir / InRelativeJsonPath;

	FString JsonString;
	if (FFileHelper::LoadFileToString(JsonString, *FullConfigPath))
	{
		UE_LOG(LogFabServerFlow, Log, TEXT("[Agent] 配置文件加载成功: %s"), *FullConfigPath);
		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			UE_LOG(LogFabServerFlow, Log, TEXT("[Agent] JSON解析成功: %s"), *FullConfigPath);
			if (JsonObject->HasField(TEXT("agent_meta")))
			{
				TSharedPtr<FJsonObject> MetaObj = JsonObject->GetObjectField(TEXT("agent_meta"));
				if (MetaObj.IsValid())
				{
					Name = MetaObj->HasField(TEXT("name")) ? MetaObj->GetStringField(TEXT("name")) : 
						  (MetaObj->HasField(TEXT("display_name")) ? MetaObj->GetStringField(TEXT("display_name")) : TEXT("Agent"));

					if (MetaObj->HasField(TEXT("icon")))
					{
						FString RelIconPath = MetaObj->GetStringField(TEXT("icon"));
						FString FullIconPath = PluginRootDir / RelIconPath;
						AvatarBrush = MakeShared<FSlateImageBrush>(FullIconPath, FVector2D(64, 64));
					}
				}
			}

			TArray<FString> Instructions;
			if (JsonObject->HasField(TEXT("system_instruction")))
			{
				const TArray<TSharedPtr<FJsonValue>>* InstructionArray;
				if (JsonObject->TryGetArrayField(TEXT("system_instruction"), InstructionArray))
				{
					for (auto Value : *InstructionArray) Instructions.Add(Value->AsString());
				}
				else Instructions.Add(JsonObject->GetStringField(TEXT("system_instruction")));
			}
			SystemPrompt = FString::Join(Instructions, TEXT("\n"));

			if (JsonObject->HasField(TEXT("allowed_tools")))
			{
				UE_LOG(LogFabServerFlow, Log, TEXT("[Agent] 解析 allowed_tools 字段"));
				const TArray<TSharedPtr<FJsonValue>>* ToolArray;
				if (JsonObject->TryGetArrayField(TEXT("allowed_tools"), ToolArray))
				{
					for (auto Value : *ToolArray)
					{
						AllowedTools.Add(Value->AsString());
						UE_LOG(LogFabServerFlow, Verbose, TEXT("[Agent] allowed_tool: %s"), *Value->AsString());
					}
					UE_LOG(LogFabServerFlow, Log, TEXT("[Agent] allowed_tools 解析完成, 数量: %d"), AllowedTools.Num());
				}
			}
		}
	}
	else
	{
		UE_LOG(LogFabServerFlow, Warning, TEXT("[Agent] 配置文件加载失败: %s"), *FullConfigPath);
	}

	if (Name.IsEmpty()) Name = TEXT("Agent");
	ContextHistory.Add(CreateMessageObject(TEXT("system"), SystemPrompt));
	UE_LOG(LogFabServerFlow, Log, TEXT("[Agent] 构造完成: Name=%s, AllowedTools=%d"), *Name, AllowedTools.Num());
}

void FUmgMcpAgent::Answer(TSharedPtr<FJsonObject> InNewSubstance)
{
	UE_LOG(LogFabServerFlow, Log, TEXT("Node: Agent.Answer"));
	UE_LOG(LogFabServerFlow, Log, TEXT("[Agent] Answer: 收到新消息，有效=%d"), InNewSubstance.IsValid() ? 1 : 0);
	if (InNewSubstance.IsValid())
	{
		ContextHistory.Add(InNewSubstance);
	}
	Send();
}

void FUmgMcpAgent::Send()
{
	UE_LOG(LogFabServerFlow, Log, TEXT("Node: Agent.Send"));
	UE_LOG(LogFabServerFlow, Log, TEXT("[Agent] Send: 发送上下文历史，条数=%d"), ContextHistory.Num());
	// 打印上下文详细内容
	for (int32 i = 0; i < ContextHistory.Num(); ++i)
	{
		const TSharedPtr<FJsonObject>& Msg = ContextHistory[i];
		if (!Msg.IsValid())
		{
			UE_LOG(LogFabServerFlow, Warning, TEXT("[Agent] Send上下文[%d]: 无效消息对象"), i);
			continue;
		}

		FString Role = TEXT("(无role)");
		Msg->TryGetStringField(TEXT("role"), Role);

		FString Content = TEXT("(无content字段)");
		if (Msg->HasTypedField<EJson::String>(TEXT("content")))
		{
			Msg->TryGetStringField(TEXT("content"), Content);
		}
		else if (Msg->HasTypedField<EJson::Array>(TEXT("content")))
		{
			const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
			if (Msg->TryGetArrayField(TEXT("content"), Arr) && Arr && Arr->Num() > 0)
			{
				const TSharedPtr<FJsonObject>* FirstObj = nullptr;
				if ((*Arr)[0].IsValid() && (*Arr)[0]->TryGetObject(FirstObj) && FirstObj && (*FirstObj).IsValid() && (*FirstObj)->HasTypedField<EJson::String>(TEXT("text")))
				{
					(*FirstObj)->TryGetStringField(TEXT("text"), Content);
				}
				else
				{
					Content = TEXT("(content为数组，首项无text)");
				}
			}
			else
			{
				Content = TEXT("(content数组为空)");
			}
		}

		FString ContentPreview = Content.Left(60).ReplaceCharWithEscapedChar();
		UE_LOG(LogFabServerFlow, Log, TEXT("[Agent] Send上下文[%d]: role=%s, content=%s%s"), i, *Role, *ContentPreview, Content.Len() > 60 ? TEXT("...") : TEXT(""));
	}
	UUmgMcpAiSubsystem* AiSubsystem = GEngine->GetEngineSubsystem<UUmgMcpAiSubsystem>();
	if (AiSubsystem)
	{
		AiSubsystem->Send(ContextHistory);
	}
}

void FUmgMcpAgent::Receive(const FUmgMcpAiResponse& InResponse)
{
	UE_LOG(LogFabServerFlow, Log, TEXT("Node: Agent.Receive - Success: %d"), InResponse.bSuccess);
	
	UUmgMcpActiveMessageSubsystem* ActiveSubsystem = GEngine->GetEngineSubsystem<UUmgMcpActiveMessageSubsystem>();
	if (ActiveSubsystem)
	{
		ActiveSubsystem->UpdateActiveMessageMeta(Name, LOCTEXT("AiResponseReceived", "AI response received"), false);
	}

	// 1. 实质数据同步 (注入 UI)
	// [Note] 文本现在由 Provider 通过 OnChunk 实时流式注入，此处不再重复追加
	// if (ActiveSubsystem && !InResponse.ResponseText.IsEmpty())
	// {
	// 	ActiveSubsystem->AppendActiveMessageText(InResponse.ResponseText);
	// }

	// 2. 实质归档 (History)
	TSharedPtr<FJsonObject> AssistantMsg = CreateMessageObject(TEXT("assistant"), InResponse.ResponseText);
	ContextHistory.Add(AssistantMsg);

	// 3. 实质落盘 (Session)
	if (auto* SessionSubsystem = GEngine->GetEngineSubsystem<UUmgMcpSessionManagerSubsystem>())
	{
		FUmgMcpSessionMessage AiMsg;
		AiMsg.Role = EUmgMcpSessionRole::Agent;
		AiMsg.Content = InResponse.ResponseText;
		AiMsg.AgentName = Name;
		SessionSubsystem->AddMessage(AiMsg, false);
	}

	// 4. 对话结束判定
	if (ActiveSubsystem)
	{
		ActiveSubsystem->StartNewChatMessage();
		ActiveSubsystem->ReturnToUser();
	}
}

void FUmgMcpAgent::OnMcpTaskFinished(const TArray<TSharedPtr<FJsonObject>>& Results)
{
	UE_LOG(LogFabServerFlow, Log, TEXT("Node: Agent.OnMcpTaskFinished - Results: %d"), Results.Num());

	if (Results.Num() > 0)
	{
		// UI 已开关并同意执行：拿到真实执行结果，开启 Send-Mcp-Send 闭环
		for (auto& R : Results)
		{
			ContextHistory.Add(R);
		}
		Send();
	}
	else
	{
		// 会话在本 Agent 处结束
		if (UUmgMcpActiveMessageSubsystem* ActiveSubsystem = GEngine->GetEngineSubsystem<UUmgMcpActiveMessageSubsystem>())
		{
			ActiveSubsystem->StartNewChatMessage();
			ActiveSubsystem->ReturnToUser();
		}
	}
}

TSharedPtr<FJsonObject> FUmgMcpAgent::CreateMessageObject(const FString& Role, const FString& Content, const TArray<FString>& InImages)
{
	TSharedPtr<FJsonObject> Msg = MakeShared<FJsonObject>();
	Msg->SetStringField(TEXT("role"), Role);

	if (InImages.Num() == 0)
	{
		Msg->SetStringField(TEXT("content"), Content);
	}
	else
	{
		TArray<TSharedPtr<FJsonValue>> ContentArray;
		TSharedPtr<FJsonObject> TextPart = MakeShared<FJsonObject>();
		TextPart->SetStringField(TEXT("type"), TEXT("text"));
		TextPart->SetStringField(TEXT("text"), Content);
		ContentArray.Add(MakeShared<FJsonValueObject>(TextPart));

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
		Msg->SetArrayField(TEXT("content"), ContentArray);
	}
	return Msg;
}

#undef LOCTEXT_NAMESPACE
