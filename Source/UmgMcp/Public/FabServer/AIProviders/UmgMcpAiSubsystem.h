// Copyright (c) 2025-2026 Winyunq. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "FabServer/AIProviders/IUmgMcpAiProvider.h"
#include "HAL/CriticalSection.h"
#include "UmgMcpAiSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnAiRequestStarted);
DECLARE_MULTICAST_DELEGATE(FOnAiRequestFinished);
DECLARE_MULTICAST_DELEGATE(FOnQuotaUpdated);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnConvergenceStatusChanged, bool, FString);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAiResponseReceived, const FString&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAiErrorOccurred, const FString&);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnAiToolExecutionReported, const FString&, const FString&, bool);

// struct FUmgMcpModelMetadata is forward-declared or defined in IUmgMcpAiProvider.h


/**
 * UUmgMcpAiSubsystem
 * 
 * AI 调度的中控系统，负责管理活动的 AIProvider。
 * 
 * [初始化流水线 / Initialization Pipeline]
 * 为了确保系统能从冷启动状态平滑收敛到就绪态，系统遵循以下分阶段初始化流水线：
 * 
 * Stage 1: Bootstrapping (引导层)
 * - 发生在插件模块加载时 (StartupModule)。
 * - 职责：初始化样式系统 (Style)、注册设置项 (Settings)。
 * 
 * Stage 2: State Restoration (持久化层)
 * - 发生在子系统 Initialize 时。
 * - 职责：同步加载本地 Persistence 数据（如 Token、未完成的并行规划任务）。
 * - 收敛点：内存状态恢复，但尚未与远端服务器同步验证。
 * 
 * Stage 3: Reactive Binding (响应层)
 * - 发生在 UI (SUmgMcpChatWindow) 构造时。
 * - 职责：Widget 绑定子系统事件。此时 UI 处于“等待收敛”状态。
 * 
 * Stage 4: Logical Convergence (收敛层)
 * - 发生在首次出站请求（如 FetchModelList 或首条消息发送）时。
 * - 职责：捕获响应头中的配额数据 (x-quota-remaining)。
 * - 终点：当配额信息首次更新后，系统进入 Steady State (就绪态)。
 * 
 * 设计决策：为了避免“因未知配额而禁止发送”导致的初始化死锁，系统采用“前馈开启”策略，
 * 即在未明确获取到资源耗尽证据前，默认允许逻辑通行 (LatestQuotaRatio 默认为 1.0)。
 */
UCLASS()
class UUmgMcpAiSubsystem : public UEngineSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** [实质派] 直接发送消息数组（排毒版核心链路） */
    void Send(
        const TArray<TSharedPtr<FJsonObject>>& Messages
    );

    /** 切换当前的 AI 提供商 */
    void SetActiveProvider(const FString& ProviderName);

    /** 获取模型列表 (便捷访问) */
    void FetchModelList(TFunction<void(bool bSuccess, const TArray<FUmgMcpModelMetadata>& Models, const FString& Error)> OnComplete);

    /** 获取最新的配额剩余百分比 (0.0~1.0) */
    float GetQuotaRatio() const { return LatestQuotaRatio; }

    /** 更新配额信息 (Manual set, e.g. on 429) */
    void SetQuotaRatio(float NewRatio) 
    { 
        LatestQuotaRatio = NewRatio; 
        if (NewRatio <= 0.0f) LatestQuotaRemaining = 0.0f; // 物理对齐：比率为0则数值归零
        OnQuotaUpdated.Broadcast(); 
    }

    /** 获取当前提供商名称 (Get current provider name) */
    FString GetProviderName() const;

    /** 显式卸载所有本地模型资源（Llama, LiteRT-LM 等） */
    void UnloadAllLocalProviders();

    /** AI 请求状态代理 (AI Request Status Broadcasts) */
    FOnAiRequestStarted OnAiRequestStarted;
    FOnAiRequestFinished OnAiRequestFinished;
    FOnAiResponseReceived OnAiResponseReceived;
    FOnAiErrorOccurred OnAiErrorOccurred;
    FOnAiToolExecutionReported OnAiToolExecutionReported;
    
    /** 配额更新代理 (Quota Update Broadcasts) */
    FOnQuotaUpdated OnQuotaUpdated;

    /** [物理中断] 收敛状态变更代理 (Convergence Status Interrupts) */
    FOnConvergenceStatusChanged OnConvergenceStatusChanged;

    /** 本地输出流代理 (Local Streaming Output Broadcasts) */
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnLocalOutputReceived, const FString&);
    FOnLocalOutputReceived OnLocalOutputReceived;

    /** 是否处于 Shell 模式 (Is in Shell Mode) */
    bool IsInShellMode() const { return bInShellMode; }
    void SetShellMode(bool bEnabled) { bInShellMode = bEnabled; }


    /** 判断当前是否正在处理 AI 请求 */
    bool IsBusy() const;
    void RequestCancel();
    bool TryCancelIfBusy();
    bool IsCancelRequested() const;

    /** 检查当前对话是否已收敛（Slot 0 是否已有有效供应商） */
    bool IsConverged() const { return ProbeChain.Num() > 0 && ProbeChain[0].IsValid(); }

    /** 获取配额数值 (Get numerical quota info) */
    float GetQuotaLimit() const { return LatestQuotaLimit; }
    float GetQuotaRemaining() const { return LatestQuotaRemaining; }

    /** 更新全量配额信息 */
    void UpdateQuotaInfo(float Remaining, float Limit) 
    { 
        LatestQuotaRemaining = Remaining; 
        LatestQuotaLimit = Limit; 
        LatestQuotaRatio = (Limit > 0) ? (Remaining / Limit) : 1.0f;
        bHasQuotaData = true; // 激活数据观测状态
        
        // 广播配额更新 (Broadcast update)
        OnQuotaUpdated.Broadcast();
    }
    
    /** 检查是否已捕获到真实的物理配额数据 */
    bool HasPhysicalQuotaData() const { return bHasQuotaData; }


    /** 获取所有已缓存的模型元数据 */
    const TArray<FUmgMcpModelMetadata>& GetCachedModels() const { return CachedModels; }
    void SetCachedModels(const TArray<FUmgMcpModelMetadata>& Models) { CachedModels = Models; }

    FString GetQuotaResetTime() const { return LatestQuotaReset; }
    void SetQuotaResetTime(const FString& ResetTime) { LatestQuotaReset = ResetTime; }

public:
    TSharedPtr<IUmgMcpAiProvider> GetProviderForCurrentMode();

private:

    /** 每次发送前重建候选供应商，确保设置变更（禁用/启用 Key）实时生效。 */
    void RebuildCandidates();

    TSharedPtr<IUmgMcpAiProvider> CurrentProvider_LiteRtLm;
    
    TArray<FUmgMcpModelMetadata> CachedModels;
    FString LatestQuotaReset = TEXT("N/A");
    float LatestQuotaRatio = 1.0f; 
    float LatestQuotaLimit = 0.0f;     
    float LatestQuotaRemaining = 0.0f; 
    bool bHasQuotaData = false; // 初始处于惯性观测状态
    bool bIsBusy = false;
    bool bCancelRequested = false;
    bool bInShellMode = false;
    mutable FCriticalSection RequestStateCs;

    /** 
     * [物理收敛探测链]
     * ProbeChain[0]: 预留给“胜出者”（Active Provider），初始为 null。
     * ProbeChain[1+]: 在构造函数筛选出的“可用候选人”（Google, CLI 等）。
     */
    TArray<TSharedPtr<IUmgMcpAiProvider>> ProbeChain;
};
