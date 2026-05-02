// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "UmgMcpSessionManagerSubsystem.generated.h"

/** 消息角色枚举 */
UENUM(BlueprintType)
enum class EUmgMcpSessionRole : uint8
{
	User,
	Agent,
	System
};

/** 单条消息数据 */
USTRUCT(BlueprintType)
struct FUmgMcpSessionMessage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UmgMcp")
	EUmgMcpSessionRole Role = EUmgMcpSessionRole::User;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UmgMcp")
	FString Content;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UmgMcp")
	FString RoleString; // 冗余字段用于 Shipping 版本稳定解析

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UmgMcp")
	FDateTime Timestamp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UmgMcp")
	FString AgentName; // 仅当 Role 为 Agent 时有效

	/** 附件图片 (Base64) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UmgMcp")
	TArray<FString> Base64Images;

	FUmgMcpSessionMessage() : Timestamp(FDateTime::Now()) {}
};

/** 会话完整数据 */
USTRUCT(BlueprintType)
struct FUmgMcpSessionData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UmgMcp")
	FString SessionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UmgMcp")
	FString Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UmgMcp")
	TArray<FUmgMcpSessionMessage> Messages;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UmgMcp")
	FDateTime LastModified;

	FUmgMcpSessionData() : LastModified(FDateTime::Now()) {}
};

/** 会话索引（用于列表显示） */
USTRUCT(BlueprintType)
struct FUmgMcpSessionIndex
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UmgMcp")
	FString SessionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UmgMcp")
	FString Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UmgMcp")
	int32 MessageCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UmgMcp")
	FDateTime LastModified;
};

// 委托定义
DECLARE_MULTICAST_DELEGATE_OneParam(FOnUmgMcpSessionResumed, const FUmgMcpSessionData&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnUmgMcpMessageAppended, const FUmgMcpSessionMessage&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnUmgMcpSessionDeleted, const FString& /*SessionId*/);
DECLARE_MULTICAST_DELEGATE(FOnUmgMcpSessionChanged);

/**
 * UUmgMcpSessionManagerSubsystem
 * 会话管理实质层：掌管数据持久化与状态派发。
 * 前端（Hub/Welcome）仅响应其发出的委托通知。
 */
UCLASS()
class UMGMCP_API UUmgMcpSessionManagerSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	// 1. 数据源接口
	void ResumeSession(FString SessionId);
	void ClearActiveSession();
	void AddMessage(const FUmgMcpSessionMessage& Message, bool bBroadcast = true);
	void DeleteSession(const FString& SessionId);
	
	TArray<FUmgMcpSessionIndex> GetRecentSessions(int32 MaxCount = 10);
	const FUmgMcpSessionData* GetActiveSession() const { return ActiveSession.Get(); }

	// 3. Agent 调度接口
	TSharedPtr<class FUmgMcpAgent> GetMainAgent();

	// 2. 状态派发委托 (前端 UI 绑定于此)
	FOnUmgMcpSessionResumed OnSessionResumed;
	FOnUmgMcpMessageAppended OnMessageAppended;
	FOnUmgMcpSessionDeleted OnSessionDeleted;
	FOnUmgMcpSessionChanged OnSessionChanged;

private:
	TUniquePtr<FUmgMcpSessionData> ActiveSession;
	TSharedPtr<class FUmgMcpAgent> MainAgent;
	
	// 内部持久化辅助
	void SaveCurrentSession();
	FString GetSessionDirectory() const;
};
