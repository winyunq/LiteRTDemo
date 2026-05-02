// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "FabServer/ChatSystem/UmgMcpSessionManagerSubsystem.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "JsonObjectConverter.h"
#include "FabServer/Agent/BaseAgent/UmgMcpDefaultChatAgent.h"

TSharedPtr<FUmgMcpAgent> UUmgMcpSessionManagerSubsystem::GetMainAgent()
{
	if (!MainAgent.IsValid())
	{
		MainAgent = MakeShared<FUmgMcpDefaultChatAgent>(TEXT("Angie_Default.json"));
	}
	return MainAgent;
}

void UUmgMcpSessionManagerSubsystem::ResumeSession(FString SessionId)
{
	FString FilePath = GetSessionDirectory() / SessionId + TEXT(".json");
	FString JsonString;

	if (FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		FUmgMcpSessionData LoadedData;
		if (FJsonObjectConverter::JsonObjectStringToUStruct(JsonString, &LoadedData, 0, 0))
		{
			// 战术加固：多重保险恢复角色信息
			for (FUmgMcpSessionMessage& Msg : LoadedData.Messages)
			{
				// 1. 优先通过 RoleString 恢复
				if (Msg.RoleString.Equals(TEXT("user"), ESearchCase::IgnoreCase)) Msg.Role = EUmgMcpSessionRole::User;
				else if (Msg.RoleString.Equals(TEXT("agent"), ESearchCase::IgnoreCase)) Msg.Role = EUmgMcpSessionRole::Agent;
				
				// 2. 兜底逻辑：如果 Role 依然不确定
				if (Msg.RoleString.IsEmpty())
				{
					// 如果有 AgentName 且长度大于 1，大概率是 Agent
					if (Msg.AgentName.Len() > 1) Msg.Role = EUmgMcpSessionRole::Agent;
					// 否则一律视为 User (保证消息可见)
					else Msg.Role = EUmgMcpSessionRole::User;
				}
				
				// 3. 最终强制校准：如果内容不为空且枚举仍为默认值，确保它在 UI 层有对应映射
				if (Msg.Content.Len() > 0 && Msg.RoleString.IsEmpty())
				{
					Msg.RoleString = (Msg.Role == EUmgMcpSessionRole::Agent) ? TEXT("agent") : TEXT("user");
				}
			}

			ActiveSession = MakeUnique<FUmgMcpSessionData>(MoveTemp(LoadedData));
			
			// 广播变更
			OnSessionChanged.Broadcast();
			OnSessionResumed.Broadcast(*ActiveSession);
		}
	}
}

void UUmgMcpSessionManagerSubsystem::ClearActiveSession()
{
	ActiveSession = nullptr;
	OnSessionChanged.Broadcast();
}

void UUmgMcpSessionManagerSubsystem::AddMessage(const FUmgMcpSessionMessage& Message, bool bBroadcast /*= true*/)
{
	const bool bWasInvalid = !ActiveSession.IsValid();
	if (bWasInvalid)
	{
		ActiveSession = MakeUnique<FUmgMcpSessionData>();
		ActiveSession->SessionId = FGuid::NewGuid().ToString();
		ActiveSession->Title = TEXT("New Chat");
		
		OnSessionChanged.Broadcast();
	}

	// 战术加固：确保在持久化前，RoleString 必须正确，且 AgentName 不能为空
	FUmgMcpSessionMessage FinalMsg = Message;
	if (FinalMsg.Role == EUmgMcpSessionRole::Agent)
	{
		FinalMsg.RoleString = TEXT("agent");
		if (FinalMsg.AgentName.IsEmpty()) FinalMsg.AgentName = TEXT("Agent");
	}
	else if (FinalMsg.Role == EUmgMcpSessionRole::User)
	{
		FinalMsg.RoleString = TEXT("user");
	}
	else
	{
		FinalMsg.RoleString = TEXT("system");
	}

	ActiveSession->Messages.Add(FinalMsg);
	ActiveSession->LastModified = FDateTime::Now();

	if (bBroadcast)
	{
		OnMessageAppended.Broadcast(FinalMsg);
	}

	SaveCurrentSession();
}

void UUmgMcpSessionManagerSubsystem::DeleteSession(const FString& SessionId)
{
	FString FilePath = GetSessionDirectory() / SessionId + TEXT(".json");
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (PlatformFile.DeleteFile(*FilePath))
	{
		// 如果删除的是当前正在进行的会话，则清空内存
		if (ActiveSession.IsValid() && ActiveSession->SessionId == SessionId)
		{
			ActiveSession = nullptr;
			OnSessionChanged.Broadcast();
		}

		// 广播删除事件，通知 UI 刷新
		OnSessionDeleted.Broadcast(SessionId);
	}
}

TArray<FUmgMcpSessionIndex> UUmgMcpSessionManagerSubsystem::GetRecentSessions(int32 MaxCount)
{
	TArray<FUmgMcpSessionIndex> Indices;
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	FString Dir = GetSessionDirectory();

	TArray<FString> FoundFiles;
	PlatformFile.FindFiles(FoundFiles, *Dir, TEXT(".json"));

	for (const FString& FilePath : FoundFiles)
	{
		FString JsonString;
		if (FFileHelper::LoadFileToString(JsonString, *FilePath))
		{
			FUmgMcpSessionData LoadedData;
			if (FJsonObjectConverter::JsonObjectStringToUStruct(JsonString, &LoadedData, 0, 0))
			{
				FUmgMcpSessionIndex Index;
				Index.SessionId = LoadedData.SessionId;
				Index.Title = LoadedData.Title;
				Index.MessageCount = LoadedData.Messages.Num();
				Index.LastModified = LoadedData.LastModified;
				Indices.Add(Index);
			}
		}
	}

	// 按时间倒序排列
	Indices.Sort([](const FUmgMcpSessionIndex& A, const FUmgMcpSessionIndex& B) {
		return A.LastModified > B.LastModified;
	});

	if (Indices.Num() > MaxCount)
	{
		Indices.SetNum(MaxCount);
	}

	return Indices;
}

void UUmgMcpSessionManagerSubsystem::SaveCurrentSession()
{
	if (!ActiveSession.IsValid()) return;

	FString JsonString;
	if (FJsonObjectConverter::UStructToJsonObjectString(*ActiveSession, JsonString))
	{
		FString FilePath = GetSessionDirectory() / ActiveSession->SessionId + TEXT(".json");
		FFileHelper::SaveStringToFile(JsonString, *FilePath);
	}
}

FString UUmgMcpSessionManagerSubsystem::GetSessionDirectory() const
{
	// 战术调整：将聊天记录保存至 EXE 同级目录下的 Saved 文件夹
	// 这样在打包发给其他人前，你可以直接看到并删除这个 Saved 文件夹，确保不泄露测试信息。
	FString Dir = FPaths::Combine(FPlatformProcess::BaseDir(), TEXT("Saved"), TEXT("UmgMcp"), TEXT("Sessions/"));

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*Dir))
	{
		PlatformFile.CreateDirectoryTree(*Dir);
	}
	return Dir;
}
