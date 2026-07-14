# LiteRT-LM 纯蓝图狼人杀教学 Demo

这是一个用于教学和展示 `LiteRT-LM-Unreal` 插件的完整狼人杀 Demo。它不靠几句固定对白证明“接入成功”，而是让本地 Gemma 模型持续驱动多个独立角色，并由 Blueprint 负责排队、校验和真实结算。

本项目中“纯蓝图”指狼人杀玩法、状态机、队列、规则校验与 UMG 交互均由 Blueprint 实现。Gemma 推理仍由 LiteRT-LM 原生库执行，插件负责把模型生命周期、异步回调、上下文策略和错误隔离可靠地暴露给 Blueprint。

## Demo 要教会什么

完成一次阅读和试玩后，开发者应能理解：

1. 如何自动准备项目内的 `.litertlm` 模型并严格请求 GPU backend；
2. 为什么应为每个 AI 玩家创建一个持久 `ULiteRtLmAgent`；
3. 如何把公有信息一次性广播、把身份与技能信息保留在私有记忆；
4. 如何在原生 single-flight 限制下用 Blueprint FIFO 驱动多个独立 Agent；
5. 如何处理流式 chunk、原生工具调用、完成回调和规则校验；
6. 如何保证推理失败时权威游戏状态不被模板或随机逻辑伪造；
7. 如何用 RuntimeStatus 区分 configured backend、native resolved backend 与尚不可用的硬件遥测；
8. 如何让详细 trace 默认关闭、在需要复现问题时按需开启，同时让玩家界面只显示不会泄露身份的摘要状态。

完整 Agent API 操作步骤见 [LiteRT-LM Blueprint 接入教程](LITERTLM_BLUEPRINT_TUTORIAL_zh.md)。旧 ABI v2 的 reset/状态演进和历史 Android JSONL 方案保留在 [ABI v2 Runtime/Blueprint 教程](LITERTLM_ABI_V2_RUNTIME_BLUEPRINT_TUTORIAL_zh.md) 供迁移参考；它不是当前 `fabLiteRTLMUnreal` 的首选接入方式。Demo 发现并推动修复的问题记录在 [插件可靠性 Issue](LITERTLM_PLUGIN_ISSUES_zh.md)。

## 入口与主要资产

- 默认地图：`/Game/WerewolfShowcase/Maps/L_WerewolfShowcase`
- GameMode：`/Game/WerewolfShowcase/Core/BP_WW_GameMode`
- 游戏核心：`/Game/WerewolfShowcase/Core/BP_WW_GameCore`
- 设置存档：`/Game/WerewolfShowcase/Core/BP_WW_UserSettings`
- 主界面：`/Game/WerewolfShowcase/UI/WBP_WW_MainV5`
- 玩家卡片：`/Game/WerewolfShowcase/UI/WBP_WW_PlayerCard`
- 消息组件：`/Game/WerewolfShowcase/UI/WBP_WW_MessageRow`
- 模型：`Content/Models/gemma-4-E2B-it.litertlm`

资产名中保留的 `V5` 是兼容名称，不代表玩法仍受旧版固定席位实现限制。

## 严格 GPU 与自动加载

共享模型在 `Project Settings > Plugins > LiteRT-LM` 配置。当前 Demo 使用：

```text
Model Path         = Models/gemma-4-E2B-it.litertlm
Backend            = strict gpu（插件不提供 CPU 模式）
Max Context Tokens = 32000
Temperature        = 1.0
Top P              = 0.95
Top K              = 64
Random Seed        = true
```

Blueprint 不需要调用初始化/加载节点。创建第一个 Agent 时，Subsystem 自动异步加载共享模型，`Ask` 也会兜底触发加载。失败路径不会创建 CPU 配置，也不会静默重新以 CPU 加载。玩家不需要选择模型文件、backend 或手动推动加载；人数、身份偏好和思考选项由设置页统一处理。

### Backend 与硬件遥测的证据边界

`FLiteRtLmResult.Backend` 来自插件加载配置，是 configured backend 的回显。RuntimeStatus 另暴露 native `ResolvedBackend` 与 `bFullyAccelerated`。

因此：

- `Result.Backend == "gpu"` 可以证明 Demo 明确请求了 GPU，且该请求没有走 Demo 的 CPU fallback；
- `RuntimeStatus.ResolvedBackend == "gpu"` 来自 live native EngineSettings，比配置回显更强；strict loader 在它不匹配时销毁 Engine 并失败；
- `ResolvedBackend` 仍不能证明设备名、GPU 利用率或逐算子落点；`HardwareTelemetry` 当前明确不可用。

文档、UI 和验收记录必须把 configured backend、native resolved Engine backend 和硬件执行遥测分开呈现，不能把任意一个配置字段描述成 GPU 利用率证明。

## Agent：多角色可靠性的基础

开局时，Core 为每名 AI 玩家调用一次 `New LiteRT-LM Agent`，并把返回对象保存到与座位相同索引的数组。每个 Agent 拥有独立的系统提示、两个工具声明和规范 JSON 记忆；正常对局不会按消息销毁/重建 Agent，也不会按角色重新加载模型。

插件只维护一个共享模型和一个原生 Conversation。轮到某 Agent 时，Subsystem 从该 Agent 的规范记忆恢复当前 Conversation，然后串行执行 `Ask`。这保证了玩家视角是多个独立聊天对象，同时不伪装多个并行 GPU/KV session。

公开发言、主持人结算和逐张公开票型通过 `Append Memory Message` 一次性广播。身份、狼人队友、查验结果和药物状态只写入有权 Agent。`Ask` 只追加本轮自然任务与合法选项；不要把同一完整公屏历史既存入 Agent 又重复塞进每个 prompt。

Gemma 4 E2B 的当前制品上下文配置为 32000 token。输入、历史、工具声明与输出共享这项预算。日志应记录实际 memory/input token、prefill/decode/TTFT、总耗时和进程内存；不要调用会物化完整 KV 的 size query 作为热路径探针。

## 真实 AI 逐席 FIFO

原生 LiteRT-LM 当前是 single-flight：同一时刻只能有一个 GPU inference。Demo 不宣称物理并行，而是区分：

- 逻辑独立：每个 AI 座位都有自己的 Agent、身份、可见记忆、请求、响应和合法性结果；
- 物理串行：Subsystem 与 Blueprint 队列确保一席完成后才执行下一席。

基础流程为：

```text
BuildActorQueue
  → QueueIndex = 0
  → SendCurrentActorRequest
      OnCompleted → Parse → Validate → Apply → QueueIndex + 1
      OnFailed    → RetryCurrentActor / PausePhase
  → ResolvePhase
```

讨论阶段中，后发言者可以读取已经公开的前序发言。投票也按存活座位 FIFO 串行执行；每一票提交后立即成为公开消息并广播给各 Agent，因此后投票者能够看到本轮已经公开的票型。候选合法性仍以投票开始时冻结的存活名单为准。

聚合成“一次请求返回所有角色发言或所有票”的做法不属于本 Demo 的最终架构，因为它不能展示每个座位都进行了真实现场推理，也无法可靠隔离角色知识。

## 自由发言与节点级 MCP

公开发言必须体现本地大模型的现场回答。Core 不提供预设台词、性格/策略模板、例句、开场白、必提玩家或必答观点，也不在失败时补一条“像 AI 的”文本。每个 Agent 只保存该玩家有权看到的公开信息、私密身份/记忆和本局稳定姓名；公开发言、主持人事件与公开票型会在提交后广播给所有有权看到它们的 Agent。

Engine 固定注册两个原生工具：

- `submit_werewolf_speech(speech)`：提交最终公开发言；
- `select_werewolf_target(seat)`：提交投票、狼人袭击、预言家查验、守卫守护或女巫用药目标。

轮到发言时，Core 只追加自然任务提示：

```text
现在轮到你发言。在确定最终发言后，调用 submit_werewolf_speech 发送你想公开说的内容。
```

轮到决策时，Core 会直接说明当前动作和合法席位，并要求调用 `select_werewolf_target`。女巫提示会明确：选择本夜被袭击者表示解救，选择其他合法玩家表示使用毒药，`seat=0` 表示不使用任何药。模型没有调用当前节点要求的工具、工具名错误、参数无法解析或目标非法时，Host 不推进游戏状态，而是保持当前节点并重试/提示；重试预算耗尽才进入可诊断暂停。

工具参数只是模型提案。Blueprint 仍会检查阶段、当前 actor、存活状态、冻结候选、身份权限、守卫连续保护限制及女巫药物资源，只有 `ValidateCurrentTarget` 通过才真正提交行为。是否跳身份、诈身份、如实报身份、保持模糊、质疑或辩护则完全由模型决定。

## 完整动态狼人杀

人数从 10 人开始。设置页用 `− / +` 调整人数，没有固定产品级最大值；玩家状态、角色袋、行动队列、冻结名单和票数都按运行时数组生成。实际可承载规模仍受设备内存、模型速度和 `int32` 等技术边界约束。

身份偏好默认“随机”，并支持村民、狼人、预言家、守卫和女巫。每局仍会随机人类座位、其余身份和头像；指定偏好只预留人类身份，不把整局变成固定脚本。

设置页另有“发言时启用思考”和“投票时启用思考”两个独立开关，默认均关闭。前者只影响公开发言请求，后者只影响白天放逐投票；狼人袭击、查验、守护与女巫用药保持非思考模式，避免手机端夜间流程产生不必要等待。

角色码只从与当前 actor 相同索引的 `RoleCodes` 读取。10 人规则袋固定包含 4 村民、3 狼人、预言家、守卫和女巫各 1；人数增大时再按规则扩展狼人和村民数量。2026-07-11 的无模型运行时探针实际得到 `[1,0,4,3,0,0,1,0,2,1]` 并跑完三昼夜，因此“多名 AI 都自称狼人”应先按模型响应坍缩调查，不能据此认定角色袋全员为狼。

游戏持续循环：

1. 夜晚：存活狼人逐席决定目标，预言家查验，守卫保护，女巫依据资源选择救人、毒人或跳过；人类若拥有对应身份，同样通过点玩家卡参与。
2. Blueprint 验证座位范围、存活状态、角色权限、冻结名单、守卫连续保护限制和女巫资源，再应用合法行动。
3. 白天：公开结算结果，存活玩家逐席发言；玩家可提前在输入框准备草稿，轮到人类时系统只等待一次“发送”。
4. 投票：每名存活 AI 按固定座位顺序推理并提交一票，人类点一张合法玩家卡完成自己的票；每一票都会立即以“投票者 → 目标”的公开消息显示并广播。
5. 唯一最高票被放逐；最高票并列时无人放逐。
6. 每次死亡或票决后重新统计阵营，未达到胜负条件则继续下一个昼夜。

没有“三轮后强制结束”或其他固定昼夜上限。只有真实阵营条件结束游戏：

- 存活狼人为 0：好人阵营胜利；
- 存活狼人数大于或等于其他存活人数：狼人阵营胜利。

人类死亡后进入观战，系统仍会自动主持其余角色直到产生胜者。

## 玩家操作边界

点击开始后，玩家只做两类操作：

- 发言输入框始终可编辑，可在 AI 行动或其他玩家发言时提前准备草稿；只有“发送”按钮会在轮到自己前保持禁用；
- 需要投票或执行身份行动时，点击一张高亮的合法玩家卡。

玩家不需要点击 Advance、Next Phase、Skip AI、Auto Play、Retry Inference 或模型设置按钮。阶段切换、AI 队列、重试、结算和胜负检查均由系统自动处理。

整张存活玩家卡始终保持移动端可触摸，点击后直接提交 `SubmitHumanTarget(DataSeat)`；高亮和候选状态只负责提示，游戏核心会再次验证回合、身份、座位与目标合法性，不能仅依赖 UI 禁用来保护规则。

## 失败策略：暂停，不冒充

AI 输出永远只是候选数据。只有同时满足以下条件才允许修改游戏状态：

- 回调属于当前 RequestId、当前角色和当前 phase epoch；
- native 正常完成，响应非空；
- JSON 可以解析且字段类型正确；
- 发言或目标符合该角色、阶段和存活名单规则。

加载失败、推理失败、no-done、无效 JSON 或非法目标发生时：

1. 不修改死亡、票数、身份资源或阶段状态；
2. 不移动当前 FIFO actor；
3. 在同一 GPU 配置下进行有限次数重试；
4. 仍失败则暂停当前阶段，并把原因写入 Windows Output Log；若用户已启用详细诊断，再同时写入 app-scoped JSONL。

失败路径禁止生成模板 AI 发言、随机投票、随机击杀，或立即跳到下一阶段。这些行为会把插件故障伪装成“模型正在玩游戏”，违背 Demo 的教学目的。

完全相同的规范化发言属于内容质量问题，不等同 native/runtime 错误。Core 先以 `duplicate_speech` 做一次质量重试；若最终尝试仍重复，则以 `speech_accepted_duplicate_after_retry` 提交并展示模型的真实输出，避免整局因质量坍缩永久停住。玩法多样性验收仍必须把这种运行判为失败，不能因为对局继续就宣称回答多样。

开局角色袋、数组长度或同索引身份绑定异常会进入 `setup_invariant_failed`：先保留 setup 阶段码、写 `game.inference_paused`，再进入暂停并触发公共状态刷新。它不会用数组默认值继续一局身份不可信的游戏。

暂停是系统可靠性状态，不是要求玩家手动推进游戏的玩法按钮。恢复策略和插件级队列仍是 [Issue 记录](LITERTLM_PLUGIN_ISSUES_zh.md) 中继续完善的方向。

## UMG 与信息呈现

主界面使用 SafeZone、Border、VerticalBox、HorizontalBox、ScrollBox、WrapBox、SizeBox 和 WidgetSwitcher 的 Auto/Fill 布局，不依赖 Canvas Panel 的绝对坐标。

- 玩家卡片显示头像、座位、名称、存活状态和当前可选状态；
- 点击整张合法卡片即可投票或完成夜间目标选择；
- 设置页可分别控制公开发言与白天放逐投票是否启用 Gemma thinking；
- 每条消息是独立组件，左侧显示说话人头像；
- 人类消息采用居中强调样式；
- 每张已提交选票以投票者自己的姓名和头像显示“投给 Pxx”，属于公屏信息；
- 消息观察器按 `LastSpeeches + LastSpeechEpochs` 扫描所有已提交公开发言，不用 `PhaseCode == 2` 阻断，因此最后一席发言与同链进入投票时也不会漏掉；专用 speech epoch 不会被夜间/投票目标提交更新，避免旧发言重现；
- 人数增加时 roster 按容器自动换行和滚动。

Shipping 玩家界面只保留 Engine、GPU backend、tools count 和通用请求阶段，不显示当前 AI 席位、姓名、RequestId、隐藏角色、私密记忆、候选目标、thought 或原始错误。实时状态可以说明正在进行夜间行动、白天发言或投票；已公开的 AI 消息可以显示通用 `LOCAL GPU AI` 标签。

详细 trace 默认关闭。设置页勾选“记录详细对话（调试）”后，后台可以记录：

```text
ChatAsyncTrace: RequestId=... Backend=gpu ContextPolicy=ResetBeforeRequest ContextReset=1 ...
```

其中 `Backend=gpu` 仍表示 configured backend。`ResolvedBackend` 应从 RuntimeStatus 单独读取；硬件利用率遥测当前不可用。

Android JSONL 位于应用 persistent download/external-files 目录，实际位置以 `RuntimeStatus.DiagnosticsPath` 为准；Windows 通常位于：

```text
<Project>/Saved/LiteRTLM/litertlm-<session>.jsonl
```

日志包含完整 Agent 上下文、输入、输出与工具调用，可能泄露隐藏身份，因此不是公屏或普通玩家日志。取消勾选会立即停止新记录并清空待写队列；关闭状态不复制完整上下文、不创建写盘线程。

为定位“对话一段时间后暂停/崩溃”是否源于上下文或内存增长，启用后的日志记录 memory/input/output、token、prefill/decode/TTFT/总耗时、backend、工具调用、结构化错误和进程内存。写盘线程按批次 flush，待写内容最多 32 MB，溢出时丢弃诊断记录而不是让日志拖垮推理。

Windows DXGI 的 local-segment budget/usage 是整个 UE 进程的资源指标，不能证明 LiteRT 某个算子落在 GPU；Android 当前写 `gpu_vram_telemetry_available=false`，不会伪造显存数字。

## 运行与人工验收

1. 用 Unreal Engine 5.8 打开 `LiteRTDemo.uproject`。
2. PIE 进入 `/Game/WerewolfShowcase/Maps/L_WerewolfShowcase`。
3. 确认设置页自动恢复人数和身份偏好、显示两个思考开关和默认关闭的详细对话日志开关，并自动准备模型。
4. 确认 Project Settings 配置项目模型、32000 context 与官方采样参数，RuntimeStatus 显示严格 GPU；每个狼人杀 Agent 声明两个工具，代码和蓝图中不存在 CPU fallback 分支。
5. 选择至少 10 人开始游戏，确认人类座位和角色按设置随机/预留。
6. 确认 AI 发言和投票按座位逐个出现，每席有独立请求，而不是一次聚合文本；每一票都公开显示投票者与目标，并进入后续 Agent 的公开记忆。
7. 确认投票通过点击玩家卡完成，且人类特殊身份能参与对应夜间行动。
8. 跨多个昼夜游玩，确认只有真实阵营胜负条件结束游戏。
9. 注入缺失工具、错误工具、非法参数或推理失败，确认状态不变、当前 actor 不推进、没有模板或随机淘汰，并在重试耗尽后暂停。
10. 在玩家 UI 中核对 Engine、GPU backend 与 tools count；确认实时状态没有显示当前推理席位、RequestId、候选目标、thought 或隐藏角色。需要完整上下文证据时临时开启详细日志，复现后立即关闭。
11. 核对 `LoadedToolsCount == 2`。发言节点必须调用一次 `submit_werewolf_speech`；夜间/投票节点必须调用一次 `select_werewolf_target`，且只有 Blueprint 规则校验通过才能 Apply。单独验证女巫提交 `seat=0` 时不消耗药物并正常结束其节点。
12. 分别切换“发言时启用思考”和“投票时启用思考”，确认只有对应请求启用 thinking，夜间身份技能始终不启用。
13. 长对局正常游玩时保持详细日志关闭；诊断性能问题时再开启，按请求序号观察上下文、进程内存和耗时，确认关闭后不再新增记录。

旧 ABI v2 Stateless soak 只保留为历史回归资料。当前 Agent 架构应验证：模型只加载一次、每个座位使用稳定 Agent、切换 Agent 后规范记忆正确恢复、原生工具调用可解析且共享 GPU 队列始终串行。这些证据仍不能替代设备利用率或逐算子硬件遥测。

若多席公开发言文字完全相同，应在开发诊断日志中核对不同 Agent、独立请求、实际 memory/input、native done、工具调用和输出 fingerprint。独立请求但 fingerprint 相同表示真实请求发生了响应坍缩，不是 UI 复制。不要用模板替换重复输出，否则 Demo 会重新变得无法证明现场推理。

## Android 单 APK

项目根目录提供可复现入口：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\Scripts\Package-AndroidSingleApk.ps1 `
  -Configuration Shipping
```

模型在打包时无损分片，首次运行时在应用持久目录重组后再交给严格 GPU 加载。环境要求、固定顺序、分片校验、安装和 Logcat 验收见 [Android 单 APK 打包流程](ANDROID_SINGLE_APK_zh.md)。

每次新包都必须重新通过结构、签名、模型字节、包内原生 ABI 和真机运行检查。当前打包脚本会从最终签名 APK 中解出 wrapper 与 `libLiteRt.so`，验证稳定 `LiteRtLm_GetApi` 导出以及 `LiteRtCreateModelFromFd` 的依赖/提供关系，避免源码目录正确但 APK 混入旧库。本文不把尚在迭代中的产物标记为正式发布件，也不以 configured/resolved backend 字段代替 Android 真机硬件利用率遥测。

当前双工具 Agent、公开票型、thinking 开关和增长诊断改动尚未登记为最终 APK，也未声称通过 Android 真机或玩法 GPU 多样性 soak。[2026-07-11 身份/多样性/隐私记录](ANDROID_DIVERSITY_FIX_BUILD_RECORD_2026-07-11.md)、[2026-07-11 初始 ABI v2/v5 记录](ANDROID_ABI_V2_V5_BUILD_RECORD_2026-07-11.md)和 [2026-07-10 ABI v1 记录](ANDROID_V6_BUILD_RECORD_2026-07-10.md)仅作为各自旧构建的历史证据。

## 已知边界

- native backend 当前 single-flight；插件 Subsystem 串行化物理请求，Demo Blueprint 仍负责发言/投票/夜间节点的玩法顺序。
- `Result.Backend` 是配置回显；RuntimeStatus 提供 native `ResolvedBackend`，但真实设备/利用率/算子遥测仍待 DLL/API 提供。
- token 数和 tokens/s 可能不可用，UI 不应伪造吞吐数据。
- Android 后台/前台切换、低内存恢复和定向取消仍需继续加强。
- 模型可能输出错误 JSON 或错误判断；规则校验、有限重试和暂停用于保证游戏状态可靠，而不是保证每次推理内容都正确。

这些边界及推荐 API 详见 [LiteRT-LM Unreal 插件可靠性 Issue 记录](LITERTLM_PLUGIN_ISSUES_zh.md)。
