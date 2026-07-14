# LiteRT-LM ABI v2：严格 GPU、多角色 Conversation 与 Android 诊断教程

本文给出当前 `LiteRT-LM-Unreal` 插件的可审计最小接入方式。它适用于狼人杀等多角色 Blueprint Demo，也适用于单聊天助手。头文件 `litert_lm_wrapper.h`、实际 DLL/SO 导出和 `FLiteRtLmRuntimeStatus` 是最终事实来源。

## 1. 先区分四个概念

| 概念 | 生命周期 | 当前实现 |
| --- | --- | --- |
| Engine | 模型权重、已解析 backend、工具 preface | 通常在应用入口创建一次，对局结束后仍可复用 |
| Conversation | 当前逻辑对话和历史 | Engine 创建时产生；独立角色请求前由 `ResetConversation` 重建 |
| KV Cache | Conversation 的推理缓存 | 属于当前 Conversation，不是 Agent 句柄 |
| SessionOwner | Unreal 请求归属和生命周期 key | 不是原生 Conversation，也不会自动隔离 NPC |

当前 wrapper 是一个进程级 Engine、一个活跃 Conversation、一个物理推理通道。多人游戏可以有很多逻辑角色，但必须 FIFO 串行调用。

## 2. 唯一正确的 Conversation reset

ABI v2 的重置入口是：

```cpp
int LiteRtLm_ResetConversation(void* engine_ptr);
```

成功 reset 会：

- 保留 Engine；
- 保留模型权重和已解析 backend；
- 保留 `CreateEngine` 时载入的完整 MCP tools preface；
- 重建 Conversation；
- 更新 `ConversationGeneration`；
- 清除旧角色的逻辑历史。

必须明确：

```cpp
LiteRtLm_SetKVCache(nullptr, 0)
```

**不是 Conversation reset。** 当前 ABI 对它的定义是保留逻辑历史并执行批量 Prefill。不能用它证明上下文已清空，也不能把它包装成 `ResetBeforeRequest`。

因此独立请求的原子顺序必须是：

```text
取得进程级 operation
  → LiteRtLm_ResetConversation(Engine)
  → 检查返回值为 0
  → 读取/核对 ConversationGeneration
  → AppendHistoryMessage（若本次显式提供）
  → AppendUserMessage
  → RunInference
  → WaitUntilDone
  → 释放 operation
```

不要在 Blueprint 中拼接：

```text
Reset LiteRT-LM Conversation → Send LiteRT-LM Chat Async
```

这是两个可交错操作。多 Agent 应直接使用 `Send LiteRT-LM Stateless Chat Async`，让插件在同一 operation 内完成 reset、append 和 inference。

## 3. Strict GPU 的准确含义

狼人杀 Demo 的加载配置：

```text
Load LiteRT-LM Project Model Async
  ModelFileName    = gemma-4-E2B-it.litertlm
  bUseAutoConfig   = false
  Backend          = gpu
  bOptimizeShader  = true
  bEnableStreaming = true
  ToolsJson        = 仅含 werewolf_choose_target 的固定数组
```

Strict GPU 要同时满足：

1. `bUseAutoConfig=false`；
2. `Backend=gpu`；
3. 失败路径没有 CPU 重试；
4. native `ResolvedBackend` 与 configured `gpu` 一致，否则销毁本次 Engine 并失败；
5. UI 不把 `ConfiguredBackend` 冒充实际硬件利用率。

证据边界：

- `ConfiguredBackend=gpu`：证明应用请求了 GPU；
- `ResolvedBackend=gpu`：来自当前 native EngineSettings，比配置回显更强，证明 Engine 解析为 GPU backend；
- `HardwareTelemetry=Unavailable`：说明当前 ABI 仍没有设备名、GPU 利用率或逐算子落点；
- `GetAvailableBackends`：只证明 DLL 的能力集合，不证明当前 Engine 或本次请求使用了什么。

产品 UI 应写成：

```text
配置后端：gpu
Native Engine backend：gpu
硬件执行遥测：不可用
CPU fallback：禁用
```

不要只显示“真实 GPU 正在运行”。

## 4. ToolsJson 必须在 CreateEngine 时固定

MCP tools 是 Engine 配置，不是某个角色的临时消息。若应用需要工具：

1. 在应用入口生成完整 `ToolsJson`；
2. `ToolsJson` 必须是非空 JSON 数组；
3. 通过 `Load LiteRT-LM Project Model Async` 的 `ToolsJson` pin 传入；
4. native 在 `CreateEngine` 时解析并建立 tools preface；
5. 加载后核对 `LoadedToolsCount` 与应用期望值；
6. 对局中所有角色复用同一个 tools schema，不得按角色改变。

没有工具需求时传空字符串，并期望 `LoadedToolsCount=0`。空工具不是错误，也不影响普通文本推理。

狼人杀 Demo 有且只有 1 个工具：`werewolf_choose_target(target, reason?)`，因此启动验收必须要求 `LoadedToolsCount == 1`。工具 preface 对整个 Engine 静态存在，但 Core 按请求强制使用语义：

- 公开发言是 `Forbidden`：只允许最小 `{ "speech": string }` JSON，任何 native tool call 都以 `unexpected_tool_call` 拒绝；
- 夜间行动和放逐投票是 `Required`：必须恰好调用一次 `werewolf_choose_target`，0 次、多个或错误名称都拒绝；
- Host 解析 tool call 的 `arguments.target` 后必须再调用 `ValidateCurrentTarget`，不能把可解析整数直接当作合法游戏行动。

“Forbidden/Required”是当前 Blueprint Core 对每次返回结果执行的语义约束，不是通过重载 ToolsJson 实现。发言 prompt 没有预设台词、策略模板或例句；模型可以自由跳身份或诈身份，普通角色声明不属于错误。

若在请求阶段才传入与 Engine 不同的 `ToolsJson`，插件只能重载 Engine 才能改变 tools preface。这会重新加载数 GB 模型、改变 `EngineGeneration`，并可能在移动设备上造成冻结或内存峰值。狼人杀 Demo 不应走这条路径。

推荐把以下值在开始游戏前同时验证：

```text
RuntimeStatus.LoadedToolsCount == ExpectedToolsCount
RuntimeStatus.ToolsSchemaHash  == 本次固定 ToolsJson 的 hash
RuntimeStatus.EngineGeneration > 0
```

## 5. Engine 加载一次，每个角色只重建 Conversation

正确生命周期：

```text
应用/主界面 Construct
  → 创建固定 ToolsJson
  → Load Project Model Async
  → OnCompleted
  → 验证 RuntimeStatus
  → 才允许开始游戏

每个 AI 角色
  → 入 Blueprint FIFO
  → Send Stateless Chat Async
      内部 ResetConversation
      内部 Append + Inference
  → 验证结果
  → Apply

应用退出
  → Unload Model
```

一局正常游戏中：

- `EngineGeneration` 应保持不变；
- 每次成功 Stateless reset 后，`ConversationGeneration` 应变化；
- 不要每换一个角色就 Unload/Load Engine；
- 不要把 `SessionOwner` 当作 Conversation handle；
- 每个 prompt 必须显式包含该角色有权看到的信息。

Demo 的每次角色请求都使用 `ResetBeforeRequest`，并只注入经过转义的本日最近 1,000 字公开讨论。完整公开消息仍由 UI 保存；这个上限只约束模型 prompt，用来避免 2048-token 上下文随对局无限增长。

若 `EngineGeneration` 在对局中增加，应检查是否误调用 Load、是否按角色改变了 `ToolsJson`，或是否进入了显式恢复流程。

## 6. RuntimeStatus 字段与证据等级

通过 Blueprint 节点 `Get LiteRT-LM Runtime Status` 获取快照。玩家可见/Shipping UI 必须用 `Format LiteRT-LM Public Runtime Status` 与 `Format LiteRT-LM Public Diagnostic Log Status`：它们保留 Engine/backend/tools/Conversation/reset 与通用请求生命周期证据，但不会显示 `ActiveRequestId`、`LastError` 或 `DiagnosticLastWriteError` 原文。旧 `Format LiteRT-LM Runtime Status` 和 `Format LiteRT-LM Diagnostic Log Status` 仅用于开发者诊断界面；业务判断仍应读取原字段。

| 字段 | 证明什么 | 证据等级与限制 |
| --- | --- | --- |
| `EngineState` | UE 包装层生命周期：Unloaded/LoadingEngine/Loaded/LoadFailed/Quarantined | UE 状态，不是硬件遥测 |
| `bEngineLoaded` | 当前有非空 native Engine handle | UE 直接观察 |
| `bReadyForRequests` | Engine 已加载、operation 空闲且未 quarantined | UE 调度状态 |
| `bRequestBusy` | 全局 operation 正在被请求或生命周期操作占用 | UE 原子状态 |
| `EngineGeneration` | 经过 backend/tools 校验的成功 Engine 创建次数 | UE 计数；可发现中途重载 |
| `ConfiguredBackend` | 调用方传给 `CreateEngine` 的值 | 配置意图 |
| `ResolvedBackend` | `LiteRtLm_GetResolvedBackend(Engine)` | native EngineSettings 证据；仍不是 GPU 利用率 |
| `BackendEvidence` | `ResolvedBackend` 的来源说明 | 描述字段 |
| `bCpuFallbackAllowed` | Unreal strict workflow 是否允许 CPU fallback | 宿主策略 |
| `HardwareTelemetry` | 当前是否有设备/利用率/算子遥测 | 当前应为 unavailable |
| `NativeApiVersion` | 当前 DLL/SO ABI 版本 | native 导出 |
| `NativeBuildInfo` | 当前 native 构建标识 | native 导出 |
| `LoadedToolsCount` | Engine 实际报告的 tools preface 数量 | native Engine 证据 |
| `ToolsSchemaHash` | UE 配置侧 `ToolsJson` 的 CRC | 配置一致性辅助；不能替代 native count |
| `ConversationGeneration` | 当前 native Conversation 代数 | native reset 证据 |
| `ConversationHistoryMessages` | native 当前历史条数；`-1` 表示不可用 | native 查询 |
| `LastResetState` / `LastResetCode` | 最近一次 `ResetConversation` 是否成功及返回码 | UE 对 native 调用结果的记录 |
| `LastError` | 最近生命周期/reset 错误 | 诊断文本，不应单独驱动玩法 |
| `DiagnosticLogDisplayPath` | 当前日志显示路径 | 文件位置证据 |
| `bDiagnosticExportHealthy` | Android 公共 MediaStore 镜像是否仍可写 | 日志导出状态，不是模型状态 |

建议开始按钮至少检查：

```text
bEngineLoaded == true
bReadyForRequests == true
ConfiguredBackend == "gpu"
ResolvedBackend == "gpu"
bCpuFallbackAllowed == false
LoadedToolsCount == ExpectedToolsCount
```

## 7. Blueprint Demo 最小流程

### 加载阶段

```text
Construct
  → BuildFixedToolsJson
  → Load LiteRT-LM Project Model Async
      OnProgress  → 更新模型准备进度
      OnCompleted → Get Runtime Status → Validate → Enable Start
      OnFailed    → 显示 ErrorMessage，禁止开始，不尝试 CPU
```

### 多角色阶段

```text
BuildActorQueue
  → QueueIndex = 0
  → SendCurrentActor
      → Send LiteRT-LM Stateless Chat Async
          RequestId = Match/Day/Phase/Seat/Attempt
          SessionOwner = 稳定的 GameCore Actor
          OnChunk     → 只更新“正在推理”和 chunk 计数
          OnCompleted → 核对 RequestId/reset/backend
                         → Parse JSON
                         → Validate game rule
                         → Apply
                         → QueueIndex + 1
          OnFailed    → 保持权威状态和 QueueIndex
                         → 有界重试一次
                         → 再失败则暂停
  → Queue empty → ResolvePhase
```

`OnChunk` 不能直接改变投票、死亡或技能资源。只有当前 RequestId 的完整响应通过 JSON 和玩法规则校验后才能 Apply。失败时禁止模板发言、随机投票、随机击杀或自动跳阶段。

若开局角色袋或并行数组不满足同索引不变量，Core 会保留 setup 阶段并以 `setup_invariant_failed` 写 `game.inference_paused`，再进入可见暂停；不能用默认数组值继续。规范化重复发言则是质量问题：先重试一次，最终仍重复时提交真实输出并以 `speech_accepted_duplicate_after_retry` 标记，玩法多样性验收仍判失败。

## 8. Android JSONL 日志

诊断系统始终写一份 app-scoped 主日志；Android 10 及以上还通过 MediaStore 镜像到：

```text
内部存储 / Download / LiteRTDemo /
LiteRTDemo-diagnostics-<session>.jsonl
```

中文文件管理器通常显示：

```text
下载 / LiteRTDemo /
```

该方案不申请 `READ_EXTERNAL_STORAGE`、`WRITE_EXTERNAL_STORAGE` 或 `MANAGE_EXTERNAL_STORAGE`。Android 8/9 只保证 app-scoped 主日志。

检查：

```text
RuntimeStatus.bDiagnosticExportHealthy == true
RuntimeStatus.DiagnosticLogDisplayPath  == Download/LiteRTDemo/...
```

若 `bDiagnosticExportHealthy=false`，不要告诉用户“日志已导出”。应用仍应保留 `Saved/Diagnostics` 主日志，并显示 RuntimeStatus 返回的实际路径。

JSONL 每行都是独立 JSON。app-scoped 主日志记录 session、RequestId、EngineGeneration、ConversationGeneration、reset code、backend 证据、tools count、阶段、角色路由、chunk 计数和错误；默认不要写完整私密 prompt、身份记忆或用户输入。Android `Download/LiteRTDemo` 公共副本由严格白名单重建，可保留 backend/reset/generation/tools、context/lifecycle 长度/耗时计数、请求的 temperature/top-p/top-k、`sampling_controls_evidence=submitted_through_abi_native_honoring_unverified`、`constraint_applied`、数值 `tool_call_count`、`native_reset_elapsed_ms`、KV-size-unavailable 标志/原因，以及稳定 reason/day/phase/attempt。它明确删除 request/native request id、seat、mode、原始 error、候选摘要、`first_tool_name`、`response_fingerprint`、private/prepared/model path 和未知未来字段，避免公共日志成为隐藏夜间行动的枚举侧信道。

### 8.1 上下文、耗时和内存增长证据

插件不会把 prompt、历史正文或模型回复正文写入 JSONL。每次 `inference.request_prepared` 与 `inference.request_finished` 只追加以下内容无关指标；`game.inference_paused` 会在私有主日志中通过 `native_request_id` 继承同一请求的指标：

- `history_message_count`、`history_json_length`、`history_content_char_count`；
- `user_message_count`、`user_json_length`、`user_content_char_count`、`prompt_content_char_count`；
- `conversation_history_telemetry_available`、`conversation_history_messages_before_prepare`、`conversation_history_messages_at_submit`、`conversation_history_messages_after_completion`、`conversation_history_growth_messages`；
- `kv_cache_size_telemetry_available=false` 与 `kv_cache_size_telemetry_reason=native_size_query_materializes_full_cache_disabled_in_request_path`；
- `max_output_tokens`、`constraint_applied`、`callback_count`、`response_text_length`、`response_json_length`、`tool_call_count`、`callback_output_metrics_final`；`first_tool_name` 只留私有主日志；
- `submit_elapsed_ms`、`wait_elapsed_ms`、`elapsed_ms`；
- `generated_token_count_available` / `generated_token_count` 与 `tokens_per_second_available` / `tokens_per_second`；
- `process_physical_used_bytes`、`process_peak_physical_used_bytes`、virtual memory 对应字段和请求内 growth；
- 最近 32 个请求的 window size、prompt/output 字符总量、总耗时和最大 peak-memory growth。

若 ABI 没有真实生成 token 数或 TPS，对应 `*_available=false`；插件不会用字数或耗时伪造。Windows 上可以查询 DXGI 进程 local-segment budget/usage 时，会记录 `gpu_vram_telemetry_available=true` 及 budget/usage；它仍只是整个 UE 进程的资源指标，不能隔离模型分配，也不是 LiteRT 算子落在 GPU 的证据。Android 当前明确记录 `gpu_vram_telemetry_available=false` 与 `unsupported_platform`，不填假数字。

当前 native `GetKVCache(nullptr,&size)` 不是 O(1) 元数据查询：它会完整物化/复制 KV cache。插件因此在 reset、submit、completion 热路径完全不调用它；否则诊断探针可能自己制造内存峰值或 OOM。若要恢复 KV 大小统计，必须先在 wrapper 增加独立、不物化缓存的 O(1) size ABI。

排查“越聊越慢/随后停止”时，按 `request_ordinal` 排序并同时观察 `prompt_content_char_count`、`conversation_history_messages_at_submit`、`elapsed_ms`、`process_physical_used_bytes` 与 peak growth：如果前两者持续增长且内存/耗时同步抬升，才有上下文膨胀证据；若 Stateless 请求每次 reset 后 history 保持有界而进程内存仍持续增长，则优先检查 native cache/allocator 生命周期。确认 KV size telemetry 一直为不可用即可，不要临时加入 `GetKVCache` probe。仅凭一次暂停或 `ResolvedBackend=gpu` 不能断言显存耗尽。

## 9. 排错

### 模型加载失败

查看：`EngineState`、`LastError`、`NativeApiVersion`、`NativeBuildInfo`。若 `ConfiguredBackend=gpu` 但 `ResolvedBackend` 为空或不一致，strict workflow 应直接失败，不能切 CPU。

### MCP tools 数量为 0

若预期有工具，说明 `ToolsJson` 没有在初始 Load pin 传入、JSON 不是非空数组，或 native preface 校验失败。修复启动配置后重新创建 Engine，不要在角色请求时临时注入。

### 第二个角色继承第一个角色的内容

确认使用的是 Stateless Async；检查 `LastResetState=Succeeded`、`LastResetCode=0`，以及 `ConversationGeneration` 是否在请求前变化。不要用 `SetKVCache(nullptr,0)` 排错，它不是 reset。

### 同帧只有一个请求成功

这是 single-flight。所有角色必须进入一个 Blueprint FIFO；busy 不是让调用方无限立即重试的信号。

### EngineGeneration 在对局中增加

检查是否反复加载模型、是否改变 ToolsJson、是否把角色切换误实现为 Engine reload。正常角色切换只应改变 `ConversationGeneration`。

### 手机像是崩溃或卡死

先从 `Download/LiteRTDemo` 取最新脱敏 JSONL，查看最后事件的 phase/reason、reset、backend、RunInference/Wait、done 与长度计数。若必须按 RequestId/seat/mode 关联隐藏角色，只能取得 app-scoped 主日志（开发机 Windows 直接读取，Android 通过开发者设备/adb 取证），不能要求公共副本包含这些字段。公共导出不健康时，依据 Public RuntimeStatus 给出的实际私有路径定位主日志。完整 native 崩溃栈仍可能需要 `adb logcat -b crash` 或系统 tombstone。

用户最近截图中的公开状态其实显示 native 请求已 `Completed/Finished` 且 request idle，随后才出现暂停。这不能证明 GPU 停止；更像 Host 在 native 完成后的解析、工具、重复、规则或重试路径暂停。对应原 session 文件当前不在本机，不能把某个具体 `reason_code` 当成事实。另有相邻日志样本完成 100 次 native 请求但其中 44 个响应拥有同一 fingerprint，说明输出坍缩与 runtime 停止必须分开判断。要验证“上下文增长导致 OOM”，必须看 Conversation、process memory、DXGI（Windows）、耗时和 32 请求趋势是否同步增长；KV 大小探针已因会物化完整缓存而禁用。

### 想证明真实 GPU 利用率

`ResolvedBackend=gpu` 只能证明 native EngineSettings 选择 GPU backend。当前 `HardwareTelemetry` 明确不可用；要证明设备名、利用率或逐算子执行位置，必须扩展 native ABI，不能从配置值、RHI 名称或可用后端列表推断。

## 10. 验收清单

- Load 只执行一次，`EngineGeneration` 在对局中稳定。
- 固定 ToolsJson 在 CreateEngine 时载入，狼人杀 native tools count 恰好为 1。
- 发言没有模板/例句，允许正常跳身份和诈身份，且 `tool_call_count == 0`。
- 每个 AI 夜间行动/投票恰好调用一次 `werewolf_choose_target`，arguments 必须通过 `ValidateCurrentTarget`。
- strict GPU 没有 auto config 或 CPU fallback。
- `ConfiguredBackend` 与 `ResolvedBackend` 都是 `gpu`。
- UI 明确显示硬件遥测不可用。
- 每个角色使用唯一 RequestId 和同一个 FIFO。
- 每次独立角色请求使用 Stateless Async。
- reset 成功且 `ConversationGeneration` 变化。
- `SetKVCache(nullptr,0)` 没有被当作 reset。
- 失败不修改权威玩法状态，不生成伪 AI 行为。
- setup 不变量失败有 `setup_invariant_failed`；普通角色声明不会触发旧 `role_leak`。
- 长对局能在日志中关联 prompt/history、Conversation、耗时/TPS、constraint/tool count、进程内存和 32 请求趋势；KV size telemetry 明确禁用且不调用 materializing query；Windows DXGI 不被解释为算子落点，Android VRAM 显式不可用。
- Android 10+ 最新 JSONL 可在 `Download/LiteRTDemo` 找到，或 UI 明确显示导出失败。

本文不声称当前 Android 真机或玩法 GPU 多样性 soak 已通过；这些必须在包含当前改动的新构建上单独验收。
