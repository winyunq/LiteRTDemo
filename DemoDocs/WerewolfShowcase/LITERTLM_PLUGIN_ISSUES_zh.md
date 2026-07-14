# LiteRT-LM Unreal 插件可靠性 Issue 记录

本文记录狼人杀 Showcase 在真实 Gemma GPU 对局中发现的问题、修复和仍待完善的通用 API。模型文件、Windows DLL 与 Android SO 均是已验证输入；这里讨论的是 Unreal 包装层。

> ABI v2 更新：当前真实 reset 是 `LiteRtLm_ResetConversation(Engine)`。下文关于 `SetKVCache(nullptr,0)` 的描述保留为 ABI v1 历史故障记录；在当前 ABI 中该调用保留逻辑历史并批量 Prefill，**不是 reset**。当前接入契约见 [ABI v2 Runtime/Blueprint 教程](LITERTLM_ABI_V2_RUNTIME_BLUEPRINT_TUTORIAL_zh.md)。

## 已修复

### P0（ABI v1 历史）：Release Session 没有清空原生 conversation

现象：第一轮夜晚、讨论、投票成功，第二轮响应退化，随后 `WaitUntilDone=0` 但没有 done callback。旧蓝图的失败 fallback 在约百毫秒内连续随机淘汰玩家，视觉上像所有 AI 同时下线。

原因：`ReleaseLiteRtLmSession` 只删除 UE `AgentCacheMap`；真正的 `LiteRtLm_SetKVCache(nullptr,0)` 被注释。普通 ChatAsync 又不会建立有效的 per-agent KV 隔离，因此每次继续向同一个原生 conversation 追加消息。

当时修复先以 KV 清空缓解；ABI v2 的最终修复为：

- 新增 `ELiteRtLmContextPolicy`。
- 新增 `Send LiteRT-LM Stateless Chat Async`。
- 在取得 global operation 后、任何 history/user Append 前调用 native `ResetConversation(Engine)`。
- reset 导出缺失或返回非 0 时 fail-closed，不再使用旧 Conversation。
- `ReleaseSession` 对当前活跃原生 Conversation 执行受保护 reset。
- Result 增加 `ContextPolicy` 与 `bContextWasReset`。
- RuntimeStatus 增加 Engine/Conversation generation、reset code、resolved backend、tools count 与 native build 证据。

回归：同一模型加载下 100/100 严格配置为 `gpu` 的 Stateless 请求完成，100/100 reset 成功。这里的 `gpu` 是配置证据；实际设备遥测边界见下文。

### P0：Loader 对必要符号检查不完整

旧 Loader 只检查 `CreateEngine` 与 `RunInference`，却输出“All symbols resolved”。

ABI v2 修复：强制检查：

- `LiteRtLm_CreateEngine`
- `LiteRtLm_DestroyEngine`
- `LiteRtLm_AppendUserMessage`
- `LiteRtLm_RunInference`
- `LiteRtLm_WaitUntilDone`
- `LiteRtLm_ResetConversation`
- `LiteRtLm_GetConversationGeneration`
- `LiteRtLm_GetConversationHistorySize`
- `LiteRtLm_GetResolvedBackend`
- `LiteRtLm_GetLoadedToolsCount`
- `LiteRtLm_GetApiVersion`
- `LiteRtLm_GetBuildInfo`

缺失时列出具体 symbol 并拒绝加载；API 版本低于 2 同样拒绝。

### P0：跨阶段共享 `oneOf` 使合法 JSON 在玩法层被拒绝

历史 Windows session `20260711-003138-EEBAD418` 的夜间 P8 请求出现“推理暂停”。日志证明 native 并未停住：`R3`、`R4` 都是 `Backend=gpu`、`ContextReset=1`、done callback、`wait_result=0`、`has_error=false`，耗时约 257/262 ms。两次响应长度均为 17、fingerprint 均为 `C6EA4133`；按 `FCrc::StrCrc32` 复算，它与旧 prompt 示例 `{"speech":"自然中文"}` 匹配。

原因：所有阶段共用 `speech | target` 的 JSON Schema `oneOf`。夜间请求需要目标，但 constrained decoding 仍允许 `speech`，模型复制发言示例后通过 native JSON 约束；Blueprint 二次校验得到 `missing_target`，相同请求重试后再次生成同一对象，随后按设计 fail-closed 暂停。它是输出契约问题，不是 DLL、模型、GPU、reset 或 Blueprint Runtime Error；鉴于 wrapper 未消费 temperature/top-p/top-k，不能把重复归因于“低温已应用”。

修复：

- 讨论只使用最小 `speech` Schema，且 `additionalProperties=false`；prompt 删除所有发言模板、策略模板、例句和必答观点，工具调用为 Forbidden。
- Engine 初始加载时恰好注册 1 个 `werewolf_choose_target`；投票和夜间行动必须恰好返回 1 个该名称的 native tool call。
- Host 从 tool call 的 `arguments` 解析 `target`，随后以 `ValidateCurrentTarget` 验证 actor、阶段、存活状态、冻结候选和技能资源；失败绝不修改权威游戏状态。
- tools preface 对 Engine 静态不变，但发言 Forbidden / 关键行动 Required 由 Core 每请求检查 `tool_call_count`、名称和 arguments 强制执行。
- 首次拒绝只重试一次；再次失败进入明确的推理暂停，而不是显示“对局结束”。

回归必须覆盖：讨论的 `tool_call_count` 为 0；夜间/投票恰好一次 `werewolf_choose_target`；0 次、多次、错误名称、无效 arguments 和非法目标都被拒绝；工具 arguments 即使能解析，也必须经过 `ValidateCurrentTarget`。

### P0：多角色讨论输出坍缩，被误判为 UI 复制或 DLL 复用

2026-07-11 Windows session `20260711-011648-85202A60` 的前两轮讨论中共有 31 次彼此独立的 native GPU 请求：第 1 日 16 次、第 2 日 15 次。日志内有 31 个唯一玩法 RequestId、31 个唯一 `native_request_id`，并能一一关联到 31 次 `inference.request_prepared`、31 次成功 reset、31 次 native done callback 和 31 次 `inference.request_finished`。所有请求均为 `context_was_reset=true`、`wait_result=0`、`has_error=false`；Engine 证据为 `configured_backend=gpu`、`native_resolved_backend=GPU`、`cpu_fallback_allowed=false`。

31 条 native 完整响应的长度全部是 83，玩法层 fingerprint 全部是 `1946CBE8`，旧 Host 又把 31 条全部记录为 `game.response_accepted`。fingerprint 来自每次独立 native 完成结果，而不是 UI 控件中的字符串，因此这不是 UI 重复渲染一条缓存消息；31 次完整 native 生命周期也证明 DLL、GPU backend 和 reset 都在工作。诊断文件没有保存 83 字正文或 raw prompt/response，只保存长度、不可逆 fingerprint、请求关联和生命周期元数据。

已确认的原因集中在旧 Demo 的角色提示、身份状态和 Host 接受策略；采样不能作为已生效原因：

- 数值字段 `R` 没有映射到村民、狼人、预言家、守卫和女巫，模型无法可靠理解自己的身份。
- 旧 prompt 虽拼接了 `M`，却没有说明它是仅当前角色可见的私密记忆。
- 旧 Blueprint 为讨论、投票和夜间目标都填入 `Temperature=0.1`，但后续源码审计确认当前 native wrapper 生成路径没有消费 `temperature/top_p/top_k`。因此不能声称“低温导致坍缩”，也不能用修改这些 pin 证明修复。
- 不同座位缺少稳定姓名、人格和上次发言。旧 `T` 确实会随已公开发言增长，但只包含匿名席位文本；日志中的 `user_json_length` 持续增长正好证明上下文并未丢失。
- Host 没有按解析后的公开发言做精确去重，因此相同内容仍被提交并显示。

修复：

- 每局打乱至少 40 组候选姓名和人格，并按单一玩家索引稳定绑定；人数超过池大小时仍以席位后缀保持姓名唯一。
- 系统提示解释 `R` 的 `0..4` 映射，prompt 同时提供由 `RoleLabels[R]` 得到的 `RL`，不再只让模型猜数字角色码。
- 人类指定角色时先查找一个角色袋索引，再只删除该索引；随机角色同样只删除抽中的索引 `0`。不再使用会删掉全部同值角色的 `Remove Item`，并在角色袋长度或索引不一致时 fail-closed。
- `PrivateMemories` 与 `RoleCodes` 使用同一 actor 索引；狼人同伴、预言家查验和其他角色资源只写入有权知道它们的角色记忆。
- Core 目前为讨论与目标填入不同的 `Temperature/TopP/TopK/MaxTokens`，但只有 native 实际消费的字段才能作为行为证据。`temperature/top_p/top_k` 仍是待修 wrapper 问题，不能把 Blueprint pin 差异写成“采样已生效”。
- 每条成功发言以 `姓名(P席位): 发言` 追加到公开 transcript 和当日 `DiscussionTranscript`；模型只接收经过转义的最近 1,000 字，不需要被规定必须回应谁或说什么。
- Host 用规范化 `ParsedSpeech` key 去重，重复先记录 `duplicate_speech` 并进行一次质量重试；最终仍重复则以 `speech_accepted_duplicate_after_retry` 提交真实模型输出，但玩法多样性验收必须失败。
- 普通身份声称、跳身份和诈身份不再被 `role_leak` 误杀；Host 只以 `private_context_echo` 拒绝逐字内部控制/私密上下文标记。

这项修复属于 Demo 的 prompt、角色状态和 Host 校验，不需要把已成功的 native GPU 请求改成 CPU fallback，也不能用模板发言掩盖重复。

用户随后提供的两张“推理暂停”截图只显示公开状态：native request 已 `Completed/Finished`，operation 已 idle，Host 随后暂停。截图对应的原始 session JSONL 当前不在本机，因此不能准确指定最终 `reason_code`，更不能把它直接归因于 GPU OOM。另一个相邻日志样本完成 100 次 native 请求，但其中 44 个结果共享同一 fingerprint；它更支持输出坍缩 + 旧重复暂停策略，而不是“GPU 没有执行”。

### P1：公开实时推理状态可能泄露夜间行动者

实时 UI 曾显示当前 seat 和 RequestId。白天它们只是诊断信息，但夜间可间接暴露狼人、预言家等正在行动的席位，即使模型内容本身没有公开。

修复：公开实时状态改用 `Format LiteRT-LM Public Runtime Status` 与 `Format LiteRT-LM Public Diagnostic Log Status`，只显示通用的模型/Engine/GPU/reset/等待状态，不显示 actor seat、RequestId、native request id、targeting mode、`LastError` 或 `DiagnosticLastWriteError` 原文。暂停时通过 `PausedPhaseCode` 保留安全的夜间/发言/投票阶段，并只显示稳定 `LastInferenceReasonCode`。公开消息仍显示姓名、头像和已提交发言；其来源元数据只显示通用 `AI` 与不可逆 response fingerprint，例如 `AI · FP 1946CBE8`。完整 RequestId 仅进入 app-scoped metadata-only 主日志。日志与 UI 均不记录或显示 raw prompt、raw response、私密记忆和未提交的夜间行动内容。

### P1：普通日志可能泄漏角色私密 prompt

修复：history/user JSON 正文不再进入任何 UE 日志；`VeryVerbose` 也只输出 JSON 字符数和“content intentionally not logged”。普通 trace 只保留 RequestId、配置后端、context policy/reset、chunk、fingerprint、耗时与状态。结构化玩法日志同样坚持 `metadata_only`：不记录 prompt、身份记忆、用户输入或原始模型响应。

### P1：native 完成与玩法拒绝无法区分

旧 JSONL 只能证明 `RunInference/Wait/done`，不能说明响应随后为何被 Blueprint 拒绝。2026-07-11 的 session 因而只能从连续同座位 RequestId、相同 fingerprint 和暂停状态反推 `missing_target`。

修复：新增 Blueprint 可用的 metadata-only 结构化事件，并与插件生命周期事件写入同一 JSONL：

- `game.response_rejected`：当前常见代码为 `missing_speech`、`private_context_echo`、`duplicate_speech`、`missing_tool_call`、`unexpected_tool_call`、`invalid_tool_arguments`、`illegal_target`、`runtime_proof_failed` 或 `native_request_failed`；`missing_target` 只属于旧直接 JSON 目标路径。
- `game.inference_retry`：当前 actor 保持不变并消耗一次重试预算。
- `game.inference_paused`：预算耗尽；在 `PhaseCode` 改为暂停值前写入，保留真实失败阶段。

玩法事件的 `data` 字段为：

```text
source, content_policy, raw_model_content_logged
reason_code, request_id, day, phase, seat, mode, attempt
error, response_fingerprint, candidate_summary
```

其中固定为 `source=blueprint_host`、`content_policy=metadata_only`、`raw_model_content_logged=false`；`error` 是限长主机错误，fingerprint 不可逆，`candidate_summary` 只包含冻结候选数量和席位摘要。USTRUCT 本身没有 prompt/message/raw-response 字段，从接口层减少误用。

完整主日志位置为 Windows/Android app-scoped `<项目或程序>/Saved/Diagnostics/LiteRTDemo-diagnostics-<session>.jsonl`。Android 10+ 还通过 MediaStore 把严格白名单脱敏副本写入 `Download/LiteRTDemo/LiteRTDemo-diagnostics-<session>.jsonl`。公共副本可包含请求的 temperature/top-p/top-k 及 `submitted_through_abi_native_honoring_unverified` 标签、`constraint_applied`、数值 `tool_call_count`、`native_reset_elapsed_ms` 和 KV-size-unavailable 标志/原因，但不会包含 request id、seat、role-specific mode、原始 error、candidate summary、`first_tool_name`、`response_fingerprint`、private/prepared/model path 或未知字段；工具名和 fingerprint 可能成为枚举隐藏夜间行动的侧信道。只有 `bDiagnosticExportHealthy=true` 才能声称公共副本存在，否则必须显示 `DiagnosticLogDisplayPath` 的实际私有路径。

### P1：Blueprint 缺少可审计的独立请求节点

修复：旧 Chat Async pin 完全保留，另加 Stateless Async，避免破坏已保存蓝图。另提供 `Reset LiteRT-LM Conversation` 作为诊断节点，但正式请求不依赖“Reset + Send”两步组合。

### P1：缺少定位上下文/内存膨胀的请求级证据

旧日志有 `user_json_length`、callback、wait 和总耗时，但无法在同一请求内比较进程内存起止，也没有有界趋势窗口。发生“运行一段时间后停止”时，无法区分 prompt 持续增长、native Conversation 未清、进程 allocator/cache 增长或单次超时。

修复：插件在统一 JSONL 层为 requested、completed/failed 与 `game.inference_paused` 追加内容无关指标：history/user/prompt 字符数和 JSON 长度；Conversation history/generation 在 prepare/submit/completion 的值；最大输出 token、constraint、callback/output 字符数、`tool_call_count`/首个工具名；native reset 与 submit/wait/total 耗时、真实 token/TPS 可用性；UE 进程 current/peak physical/virtual memory；以及最近 32 个请求的 prompt/output/elapsed/peak-growth 汇总。窗口固定上限为 32，不会因长时间对局让诊断器自身无限增长。

安全审计随后发现，native `GetKVCache(nullptr,&size)` 会完整物化/复制 KV cache，不是 O(1) 元数据查询。把它放在 reset/submit/completion 热路径会制造额外峰值，甚至成为 OOM 原因。主线已移除全部 KV size probe，相关事件只写：

```text
kv_cache_size_telemetry_available=false
kv_cache_size_telemetry_reason=native_size_query_materializes_full_cache_disabled_in_request_path
```

未来只有新增独立、O(1)、不物化缓存的 native size ABI 后，才能恢复 KV size telemetry。

Windows 可通过 DXGI 查询当前进程 local-segment budget/usage 时记录真实值；字段明确标记这是整个 UE 进程的资源指标，不能隔离模型分配，也不是 LiteRT 算子落点证据。Android 当前写 `gpu_vram_telemetry_available=false` / `unsupported_platform`，不伪造显存。ABI 没有真实 token count/TPS 时同样写 availability=false，不用字数估算。

## 已确认但尚未完整解决

### P1：`temperature/top_p/top_k` 已过 UE/C ABI，但 native wrapper 未消费

`Make LiteRT-LM Sampling Params`、`FLiteRtLmSamplingParams` 和 C ABI 结构都带有 `Temperature`、`TopP`、`TopK`，插件也会把值转交给 wrapper；但当前 wrapper 的实际生成配置没有使用这三个字段。结果是 Blueprint pin 看起来可调，实际采样行为不随之改变。

这不是文档问题或模型随机性问题，而是 wrapper 实现缺口。修复前：

- 不得声称“讨论高温、目标低温已生效”；
- 不得把多样性变化或坍缩归因于这些配置值；
- 日志中的配置值只能证明调用方传入，不能证明 native sampler 应用。

当前诊断因此同时写 `requested_temperature`、`requested_top_p`、`requested_top_k` 和 `sampling_controls_evidence=submitted_through_abi_native_honoring_unverified`，把“已提交”与“native 已应用”明确分开。

推荐修复：在 native wrapper 中把三项显式写入实际 sampler/generation config，增加可查询的 applied-value/capability 证据，并用固定 prompt 的低温/高温对照回归确认参数真正影响生成。`MaxTokens`、constraint/tool behavior 仍按各自独立证据验收。

### P0：single-flight 目前是拒绝，不是插件级 FIFO

同帧启动多个请求时，插件只允许一个 global operation，其他请求会收到 busy。狼人杀已在 Blueprint 层串行排队，因此功能正确，但通用插件仍应提供：

```text
Queue LiteRT-LM Chat Async
OnQueued / OnStarted / OnChunk / OnCompleted / OnFailed / OnCancelled
```

队列应支持 RequestId、Owner 定向取消、队列位置、排队耗时和 FIFO 保证。

### P1：no-done 的恢复策略仍可进一步结构化

当前实现会停止、再次 Wait，并在不能证明 callback 静止时 poison 全局 operation，避免 use-after-free。这比继续毫秒级重试安全，但 API 仍缺少 Blueprint 可枚举错误码，例如：

- `NativeNoDoneCallback`
- `NativeTimeout`
- `EngineQuarantined`
- `ContextResetFailed`

未来应在 clean reset 后最多自动重试一次；重复 no-done 触发 circuit breaker。

### P1：Poisoned 状态缺少显式恢复 API

当 native callback 无法证明静止时，永久拒绝后续工作是安全默认值，但需要一个经过严格验证的 unload/reload recovery 流程与 Blueprint 状态查询。

### P1：StopInference 没有 RequestId

当前只能停止进程内活跃请求。加入插件 FIFO 后，应支持：

```text
CancelLiteRtLmRequest(RequestId)
CancelLiteRtLmRequestsForOwner(Owner)
```

### P1：Android 生命周期

应监听进入后台、回到前台和低内存事件：暂停队列、定向停止活跃请求、前台严格恢复 GPU。任何恢复路径都不能静默切换 CPU。

### P2：Backend 选择证据仍不等于硬件利用率遥测

`Result.Backend` 来自加载配置。ABI v2 RuntimeStatus 的 `ResolvedBackend` 来自 live native EngineSettings，strict loader 会在它与 configured `gpu` 不一致时销毁 Engine 并失败。这能证明 native backend 选择，但不能证明设备名、GPU 利用率或逐算子落点；此类证据仍需独立 hardware telemetry API。

### P2：原生生成 token count 仍可能不可用

100-request GPU soak 中 chunk、文本、fingerprint 与耗时均有效，但 native result 的 token 计数仍为 0；TPS 只有在 native done result 返回正值时才有效。插件诊断现已用 `generated_token_count_available` 和 `tokens_per_second_available` 明确区分，并且不伪造。完整解决仍需要底层 ABI 返回真实 input/output token count 和 prefill/decode 分段耗时。

## 推荐的 V2 通用类型

```cpp
enum class ELiteRtLmRequestState : uint8
{
    Queued, Running, Completed, Failed, Cancelled, Suspended
};

enum class ELiteRtLmErrorCode : uint8
{
    None,
    InvalidInput,
    ModelNotLoaded,
    ContextResetUnavailable,
    ContextResetFailed,
    NativeStartFailed,
    NativeNoDoneCallback,
    NativeTimeout,
    EngineQuarantined,
    AppSuspended
};
```

插件请求 Trace 至少应包含：RequestId、状态、错误码、是否可重试、QueueTimeMs、InferenceTimeMs、QueuePosition、NativeWaitResult、EngineGeneration、ConversationGeneration、reset code、ChunkCount、ConfiguredBackend、ResolvedBackend、LoadedToolsCount、ToolsSchemaHash 和 ResponseFingerprint。Host 语义 Trace 还应包含 day、phase、seat、mode、attempt、稳定 reason code 和候选摘要，并通过 RequestId/fingerprint 与插件事件关联；两类 Trace 都不应记录 prompt 或原始响应。

## 回归门

- 2048 或更小 context 下连续至少 100 个 Stateless 请求，全部有 done。
- 10–30 个同帧提交请求进入 FIFO，无 busy 丢失。
- 注入 no-done：只重试一次，游戏权威状态不变，无快速阶段风暴。
- 工具语义：Engine 恰好预载 1 个 `werewolf_choose_target`；讨论只能生成 `speech` 且禁止 tool call；投票/夜间必须恰好调用一次该工具；arguments 仍由 `ValidateCurrentTarget` 二次拒绝非法目标。
- 多角色讨论：不同席位拥有稳定且唯一的姓名/人格；`R` 与 `RL` 一致；私密记忆按角色隔离。不得把尚未被 wrapper 消费的 temperature/top-p/top-k 当作多样性证据。
- 自由发言：prompt 不含预设台词、策略模板、例句、必提玩家或必答观点；普通跳身份/诈身份允许；注入内部控制标记才以 `private_context_echo` 拒绝。
- 注入相同规范化 `ParsedSpeech`：先以 `duplicate_speech` 质量重试；最终仍重复时提交实际模型文本并产生 `speech_accepted_duplicate_after_retry`，多样性验收必须失败；改变 JSON 空白或标点不能绕过去重。
- 角色袋含多个相同角色时，为玩家保留一个角色只能使袋长度减少 1；剩余角色数量与索引始终匹配。
- 注入 `missing_tool_call`/`unexpected_tool_call`/`invalid_tool_arguments`/`illegal_target`：依次出现可关联的 `game.response_rejected`、`game.inference_retry`，再次失败后出现 `game.inference_paused`；日志没有 prompt 或原始响应。
- 破坏角色袋/并行数组不变量：出现 `setup_invariant_failed`、保留 setup phase 的 `game.inference_paused` 和公共暂停状态，不使用默认角色继续。
- 长对局：每请求可关联 prompt/history、Conversation、耗时/TPS、constraint/tool count、进程内存和 32 请求趋势；KV size telemetry 必须保持 unavailable 且热路径不得调用 materializing query；Windows DXGI 不被解释成算子落点，Android VRAM 明确不可用。
- 定向取消 active/queued request，其他请求继续。
- Android 推理中切后台再回来：队列暂停/恢复，绝不走 CPU。
- Shipping UI 使用两个 Public formatter 显示整理后的 RuntimeStatus 和日志路径，但不显示当前 seat/RequestId/TargetingMode/原始错误；已公开消息元数据可显示通用 `AI` 与其不可逆 fingerprint。完整 trace 写入 app-scoped 主日志并可按 RequestId 追踪；Android `Download/LiteRTDemo` 严格脱敏副本不得导出 `first_tool_name` 或 `response_fingerprint`，但可导出 constraint/tool count/reset elapsed/KV unavailable reason；二者均不含 raw prompt/response。
