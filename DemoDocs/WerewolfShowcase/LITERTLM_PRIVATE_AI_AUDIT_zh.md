# LiteRT-LM 狼人杀私密 AI 证据审计

`analyze_v6_private_ai_debug.py` 用于核对狼人杀 Demo 中一条 AI 请求的完整证据链：游戏 Host 构造的上下文、实际提交给 native 的消息、native 原样返回、以及 Host 最终接受或拒绝的结果。它还会检查后一席的 native exact message 是否包含同一昼夜发言阶段中上一席已经接受的公开发言。

这是传输与来源审计，不是语义评分器。它只证明玩法、可见事实和此前公开发言被提供给模型，并记录模型自由返回了什么；它不会要求模型引用、分析、同意或回应任何具体观点。跑题、短答、沉默式回答、公开角色宣称、撒谎或自曝都不由本分析器判失败。

这个工具读取私密日志，默认输出却是可安全粘贴到普通 Bug 报告中的脱敏摘要。只有显式传入 `--show-private` 才会输出请求 ID、模型原文、角色、名字、人格、私有记忆、工具参数和错误原文。

## 快速使用

默认读取以下目录中修改时间最新的 `*.jsonl`：

```text
Saved/Diagnostics/PrivateDebug
```

在项目根目录执行：

```powershell
python Saved/WerewolfShowcaseTools/analyze_v6_private_ai_debug.py
```

指定一个日志或另一个目录：

```powershell
python Saved/WerewolfShowcaseTools/analyze_v6_private_ai_debug.py `
  --diagnostics D:\path\to\private-session.jsonl
```

仅在可信的本机调试环境查看原文：

```powershell
python Saved/WerewolfShowcaseTools/analyze_v6_private_ai_debug.py `
  --diagnostics Saved/Diagnostics/PrivateDebug `
  --show-private
```

验证分析器自身，不需要 UE 或模型：

```powershell
python Saved/WerewolfShowcaseTools/analyze_v6_private_ai_debug.py --self-test
```

如果 `PrivateDebug` 目录还不存在，工具会返回 `INPUT_ERROR`。这表示当前构建尚未产生私密证据，不能改用公开 JSONL 冒充原文证据。

## 默认输出包含什么

默认报告只包含：

- 匿名轨迹号，例如 `T001`；
- day、phase、seat 等公开游戏坐标；
- host/native ID 的数量，不显示 ID 本身；
- 请求、响应、公开发言的字符数及 12 位 SHA-256 摘要；
- accepted、rejected、缺配对、截断、重复观察项、native failure 的计数；
- 每条请求耗时，以及 min、median、p95、max、total 汇总；
- 连续性检查的 PASS/FAIL 与脱敏摘要。

稳定的 `reason_code` 会原样显示；不符合稳定标识符格式的内容会自动替换为哈希。错误原文只在 `--show-private` 模式出现。

`--show-private` 会额外输出：

- host request ID 与 native request ID；
- native exact message；
- native exact response；
- Host 接受的公开发言；
- 隐藏角色、身份、玩家名、人格与私有记忆；
- 原始错误与连续性失败两侧的原文。

不要把 `--show-private` 输出提交到公开 Issue、聊天或构建日志。

## 联结规则

工具只使用显式 ID 联结，不按照 JSONL 中的相邻顺序猜测。

一条完整轨迹需要同时具备：

1. 一个 host request ID；
2. 一个 native request ID；
3. 至少一条把两个 ID 同时写出的桥接事件；
4. context/native exact message；
5. request 生命周期事件；
6. response/native exact response；
7. 一个 Host accepted 或 rejected 终态。

同一 host ID 指向多个 native ID、同一 native ID 指向多个 host ID、缺少任一 ID、或只有相邻事件而没有桥接 ID，都会报告为缺配对或歧义配对。

为了兼容字段演进，分析器会把 camelCase、snake_case 和常见同义字段归一化。例如：

| 语义 | 可识别字段示例 |
|---|---|
| Host ID | `host_request_id`、`hostRequestId`、`gameplay_request_id`、游戏事件中的 `request_id` |
| Native ID | `native_request_id`、`nativeRequestId`、`runtime_request_id`、inference 事件中的 `request_id` |
| Exact message | `serialized_current_messages`、`serialized_history_messages`、`native_append_sequence`、`native_exact_message`、`exact_user_message_json`、`submitted_user_message` |
| Exact response | `raw_callback_text`、`raw_callback_full_json`、`result_full_text`、`native_exact_response`、`raw_model_response` |
| 公开发言 | `accepted_text`、`accepted_public_speech`、`committed_speech`、`parsed_speech`、`speech` |
| 私密身份 | `hidden_role`、`private_role`、`identity`、`player_name`、`persona`、`private_memory` |

`message` 与 `messages` 是弱别名：只有事件名本身明确表示 context、request、prompt 或 input message 时，它们才会被当成 native exact message。这样可以避免错误消息或模型响应被误判为请求上下文。

### 当前 V6 的直接证据字段

当前版本会优先识别以下五类私密事件：

| 事件 | 审计用途 | 关键字段 |
|---|---|---|
| `private_ai.host_context` | Host 在提交前掌握的玩法与可见上下文 | `request_id`、`day`、`phase`、`seat`、`visible_public_transcript`、私密角色/人格/记忆 |
| `private_ai.native_request_submitted` | native 边界的精确输入与 host/native 桥接 | `host_request_id`、`native_request_id`、`serialized_current_messages`、`serialized_history_messages`、`native_append_sequence` |
| `private_ai.native_response_received` | 原始 callback 与完成结果 | `raw_callback_snapshot_available`、`raw_callback_text`、`raw_callback_full_json`、`result_full_text`、`result_error`、耗时字段 |
| `private_ai.host_response_accepted` | Host 实际采用的文本或动作 | `accepted_text`、`accepted_action`、`tool_name`、`tool_arguments_json` |
| `private_ai.host_response_rejected` | Host 拒绝的原文与原因 | `reason_code`、`raw_response_text`、`raw_response_json`、`error` |

`serialized_current_messages` 是当前轮实际追加参数，`serialized_history_messages` 是分离保存的历史追加参数，`native_append_sequence` 保留 append 顺序。分析器优先使用当前轮精确参数，同时保留后两者供 `--show-private` 人工核验。若事件明确声明 `recorded_at_boundary=false`、append 未完成、消息不是 exact native append arguments，或消息不是 Unreal 序列化后的内容，证据链会失败；字段完全不存在时则继续兼容旧日志。

`raw_callback_snapshot_available=false` 表示不能证明原始 callback，哪怕另有整理后的 `result_full_text`，仍会报告缺少原始响应快照。Host rejected 是有效终态；其 `error` 只解释拒绝原因，不会被误算为 native failure。

## 连续性验证

连续性检查按 Host accepted 的顺序进行，并以 `(day, phase)` 分组。对于同组中相邻的两条已接受公开发言，工具要求后一条轨迹的 native exact message 包含前一条已接受发言。

这个检查只查看后一席的**输入**。即使后一席完全不提上一席、改变话题、只回答几个字、宣称任意身份或自曝，只要原始输出被如实记录，分析器都不会把这种语义选择判为失败。它也不会通过关键词、长度或“狼人杀水平”评价回答质量。

检查的是“紧邻的上一条已接受发言”，而不是要求包含整局所有历史。这与 Demo 的有界公开 transcript 尾部策略兼容，也足以证明后一席确实收到了现场刚发生的公开信息。

比较时会：

- 解析被再次 JSON 编码的消息；
- 展开 `\uXXXX`、换行和转义引号；
- 进行 Unicode NFKC 归一化；
- 忽略空白差异；
- 保留实际汉字、标点和内容顺序。

若缺少上一席的 accepted public speech、缺少后一席 exact message、日志声明请求/响应本体发生截断，或 exact message 中找不到前一条发言，报告会明确指出，且退出码为 1。Host 主动采用有界 transcript 并设置 `public_transcript_truncated=true` 时只产生 WARN；紧邻上一席仍必须存在于后一席 exact message 中，否则连续性检查仍会失败。

## 截断、重复、失败与耗时

以下情况会被统计并列出：

- JSONL 最后一行不完整或中间存在无效 JSON；
- `truncated`、`context_truncated` 等布尔标志为真；Host 明确记录的有界 `public_transcript_truncated` 会作为非阻塞 WARN 报告；
- `omitted_*`、`dropped_*` 字符或字节计数大于零；
- 原文包含明确的 `<truncated>`、`[truncated]` 标记；
- 一条轨迹有多个 accepted/rejected 终态；
- 不同请求出现完全相同的 accepted speech 或 native response；该项仅作为 `WARN duplicate_content` 观察，不单独导致失败；
- native 事件为 failed、timeout、paused、crash，或错误字段非空、`has_error=true`、`wait_result!=0`；
- native 边界证明字段明确为 false，或 `raw_callback_snapshot_available=false`；
- request、response、context、Host 终态中的任一步缺失。

Host 的正常 `rejected` 会单独计数，但本身不等同于 native failure；狼人杀的有界重试可能合理地产生一次 Host 拒绝。若拒绝事件缺少对应 native 请求/响应，仍会作为证据链缺失失败。

耗时字段会容错读取 `elapsed_ms`、`inference_elapsed_ms`、`total_elapsed_ms`、`reset_plus_inference_elapsed_ms`、`duration_ms`、`wait_elapsed_ms`、`result_time_ms`。没有显式耗时时，会尝试用同一请求的 `monotonic_seconds` 起止值计算。

## 退出码

- `0`：联结、阶段、连续性、截断、终态唯一性及 native failure 检查均通过；Host rejected 与内容重复警告可以存在。
- `1`：日志可读，但发现缺配对、缺阶段、截断、重复/冲突终态、重复 sequence、native failure 或输入连续性失败。回答是否跑题、是否回应上一席、长短和策略质量不会产生退出码 1。
- `2`：输入目录/文件不存在，或 JSONL 中没有任何可读对象。

## 证据边界

该审计能证明“某个 Host 请求与某个 native 请求对应、native 收到的 exact message 是什么、原始输出是什么、Host 如何采纳或拒绝，以及下一席是否拿到了上一席公开发言”。它不声称模型必须利用这些上下文，也不从回答内容反推模型是否进行了某一种思考。

它不能仅凭文本证明模型内部使用了哪一个 GPU 算子，也不能替代 native backend/sampler 的 fail-closed 验证。性能与 GPU 证据仍需结合普通私有诊断中的 backend、进程内存、VRAM、wait result 和 native lifecycle 指标判断。
