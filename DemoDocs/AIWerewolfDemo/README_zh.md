# LiteRTDemo AI 狼人杀 Demo 项目文档

本文档属于 `LiteRTDemo` 项目 Demo，不属于 `Plugins/LiteRT-LM-Unreal` 插件文档。插件文档继续保留在 `Document/`；本项目 Demo 文档放在 `DemoDocs/AIWerewolfDemo/`，避免把“插件 API 文档”和“项目玩法 Demo 文档”混在一起。

网页版本：`DemoDocs/AIWerewolfDemo/index.html`

Android 打包记录：`DemoDocs/AIWerewolfDemo/ANDROID_zh.md`

## 0. 已落地实现快照

截至 2026-07-07，AI 狼人杀 Demo 已在项目资产中生成第一版 Blueprint-only 实现：

| 资产 | 类型 | 状态 |
| --- | --- | --- |
| `Content/AIWerewolf/BP_AIWerewolfDirector.uasset` | Actor Blueprint | 已实现开局、提示词构建、Gemma 调用入口、第一夜、白天发言、投票、第二夜、胜负检查和自动测试链。 |
| `Content/AIWerewolf/WBP_AIWerewolfGame.uasset` | Widget Blueprint | 已创建 UMG 占位资产，写入 UI 布局约定和按钮绑定约定；受 UMG MCP socket 问题影响，视觉树稍后补。 |
| `Content/AIWerewolf/L_AIWerewolfDemo.umap` | Demo Map | 已放置 `AI_Werewolf_Director`，运行关卡时自动执行一局确定性 AI 狼人杀烟测。 |

本阶段没有新增玩法 C++。玩法状态机、阶段函数、变量、提示词和自动测试链都在蓝图资产中。编辑器自动化脚本只用于生成 `.uasset`，不属于运行时依赖。

### 0.1 蓝图运行链

`BP_AIWerewolfDirector::Event BeginPlay` 调用：

```text
RunAutomatedAITest
  -> StartNewGame
  -> BuildGemmaDecisionPrompt
  -> RequestGemmaDecision
  -> RunNightPhase
  -> RunDayDiscussion
  -> RunVotePhase
  -> RunSecondNightPhase
  -> CheckWinState
```

确定性测试局流程：

1. 8 人局：2 狼人、1 预言家、1 女巫、4 平民。
2. 第一夜：狼人击杀 `P04 Mira`，预言家 `P03 Kai` 查验 `P02 Lin = Werewolf`。
3. 第一日：Kai 跳预言家，Lin 反驳，AI 生成投票倾向。
4. 第一轮投票：`P02 Lin` 被放逐并翻出狼人。
5. 第二夜：最后狼人 `P06 Noah` 击杀 Kai，女巫 `P05 Chen` 毒杀 Noah。
6. 胜负检查：狼人清零，好人阵营胜利。

关键蓝图变量：

| 变量 | 用途 |
| --- | --- |
| `PlayerRosterCsv` | 固定测试局座位、名称、角色和初始存活状态。 |
| `GameStateJson` | 当前阶段公开状态快照，便于 UI 和 prompt 查看。 |
| `GameLog` | 当前完整对局日志。 |
| `Prompt_GameMaster_ZH` | 中文主持/规则仲裁提示词。 |
| `Prompt_PlayerDecision_EN` | 英文 AI 玩家 JSON-only 决策提示词。 |
| `LastGemmaPrompt` | 最近一次发送给 Gemma 4 E2B 的提示词。 |
| `AITranscript` | 本轮 AI 发言/模拟响应。 |
| `Winner` | 当前胜负状态，终局为 `Villagers`。 |

### 0.2 验证记录

结构验证命令：

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'D:\UE5Project\LiteRTDemo\LiteRTDemo.uproject' `
  -NoSplash -Unattended -NullRHI -DisablePlugins=Bridge `
  -ExecutePythonScript='D:\UE5Project\LiteRTDemo\Saved\AIWerewolfTools\validate_ai_werewolf_demo.py'
```

验证通过项：

- Director/Widget/Map 资产存在。
- Director 蓝图可编译。
- Widget 蓝图可编译。
- `Gemma4E2B_Brain` LiteRT-LM 组件存在。
- 自动测试链包含 `StartNewGame`、`RunSecondNightPhase`、`CheckWinState`。
- `StartNewGame` 写入 `GameLog` 和 `GameStateJson`。
- `CheckWinState` 写入 `Winner` 和终局 `GameLog`。
- `L_AIWerewolfDemo` 中存在 `AI_Werewolf_Director`。

运行验证命令：

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'D:\UE5Project\LiteRTDemo\LiteRTDemo.uproject' `
  '/Game/AIWerewolf/L_AIWerewolfDemo' `
  -game -NullRHI -NoSplash -Unattended -DisablePlugins=Bridge -ExecCmds='quit'
```

运行日志中已出现完整蓝图执行输出：

```text
AI Werewolf / AI狼人杀: new 8-player match initialized.
Build prompt for Gemma 4 E2B: public state + private role + phase action schema.
Gemma hook: load ModelFileName and send LastGemmaPrompt through LiteRT-LM when bUseLiteRTGemma is true.
Night / 夜晚: wolves choose victim, seer checks one target, witch may save or poison.
Day / 白天: living players speak using public evidence, contradictions, claims, and vote history.
Vote / 投票: collect AI votes, exile highest vote target, append public result to GameLog.
Night 2 / 第二夜: last wolf attacks Seer; Witch poisons the last wolf.
Win check / 胜负: all wolves are dead. Villagers win.
AI Werewolf automated test complete.
```

### 0.3 UMG MCP 当前阻塞

当前可调用的 MCP 工具暴露了 `set_target_umg_asset`、`apply_layout`、`create_widget`、`bluecode_*` 等接口，但实际调用时返回：

```text
[WinError 1225] 远程计算机拒绝网络连接。
```

本地源码检查显示 `Source/UmgMcp` 当前主要是编辑器聊天面板和 LiteRT-LM provider，没有发现监听 `127.0.0.1:55557` 的 Unreal 端 socket/listener 实现。因此 `WBP_AIWerewolfGame` 先作为 Widget Blueprint 占位，已写入布局和按钮绑定约定；等 UMG MCP 的 Unreal 端服务可用后，再把视觉树和按钮事件补进去。

## 1. 当前项目状态

当前 LiteRTDemo 可以理解为一个“本地 LLM 对话 Demo”：

- `Content/Demo.umap` 是当前演示关卡。
- `AWinyunqDemoGameMode` 在 BeginPlay 中创建聊天 UI。
- `ADemoTavernHUD` 显示 LiteRT-LM 性能监控。
- `UWinyunqDialogueWidget` 展示了流式文本回调如何进入 UMG。
- `ULiteRtLmComponent` 可以挂到 Actor 上，作为每个 AI 角色的本地 LLM 会话组件。
- `ULiteRtLmBlueprintLibrary` 已提供蓝图节点，用于加载模型、发送文本/JSON 请求、设置采样参数和释放 Session。

插件更新已检查：`Plugins/LiteRT-LM-Unreal` 的 `master` 当前是最新状态。主项目仍可能显示 submodule 指针差异，这是项目记录的插件 commit 和当前插件 checkout 不一致导致的，不代表插件内部有未提交文件。

## 2. Demo 目标

目标是把 LiteRTDemo 升级为一个可学习、可调试、蓝图优先的 AI 狼人杀 Demo。

第一版目标：

1. 单机运行。
2. 一名人类玩家加若干 AI 玩家，或人类作为旁观主持。
3. AI 玩家由 LiteRT-LM 本地推理驱动。
4. 规则由蓝图状态机权威执行，AI 只返回发言、投票和夜间行动意图。
5. UI 展示座位、阶段、发言、夜间结果、投票和胜负。
6. Debug 面板展示 prompt、原始输出、解析结果、延迟和 tokens/s。

非目标：

- 第一版不做联网多人。
- 第一版不把复杂角色全部做完。
- 第一版不让 AI 直接修改游戏状态。
- 当前阶段不依赖 UMG MCP 编辑蓝图；UMG MCP 可用后再自动化创建 UI。

## 3. 推荐 MVP 规则

建议先做 6 人局：

| 角色 | 数量 | 说明 |
| --- | ---: | --- |
| 狼人 | 2 | 夜间共同选择击杀目标，白天伪装并投票。 |
| 预言家 | 1 | 夜间查验一名玩家阵营。 |
| 女巫 | 1 | MVP 先做解药，再扩展毒药。 |
| 平民 | 2 | 白天发言和投票。 |

胜利条件：

- 好人胜利：所有狼人死亡。
- 狼人胜利：狼人数量大于或等于好人数量，或所有神职死亡。

第一版可以固定角色配置，后续再做房间配置和随机板子。

## 4. 项目资产规划

### 4.1 Game Framework

| 资产 | 父类 | 职责 |
| --- | --- | --- |
| `BP_WW_GameMode` | `GameModeBase` | 创建新局、加载模型、生成玩家、创建 HUD。 |
| `BP_WW_GameState` | `GameStateBase` | 保存阶段、天数、公开座位状态和公共日志。 |
| `BP_WW_PlayerController` | `PlayerController` | 处理人类输入、鼠标和 UI 输入模式。 |
| `BP_WW_RoundManager` | `Actor` | 狼人杀规则状态机，唯一权威玩法执行者。 |
| `BP_WW_AIRequestQueue` | `Actor` 或 `Object` | 串行调度 LiteRT-LM 请求，避免多个 AI 同时推理。 |
| `BP_WW_PlayerAgent` | `Actor` | 一个座位上的玩家实体；AI 玩家挂 `LiteRtLmComponent`。 |
| `BP_WW_DebugDirector` | `Actor` | 统一收集 prompt、输出、解析错误和性能数据。 |

### 4.2 Data

| 资产 | 类型 | 内容 |
| --- | --- | --- |
| `DT_WW_Roles` | DataTable | 角色名、阵营、行动顺序、UI 颜色、可行动阶段。 |
| `DT_WW_Personas` | DataTable | AI 性格参数，如谨慎、激进、诈身份倾向。 |
| `DT_WW_PromptTemplates` | DataTable | 夜间行动、白天发言、投票和记忆摘要 prompt。 |
| `DA_WW_RuleConfig` | PrimaryDataAsset | 玩家数量、角色配置、讨论轮数、发言字数、超时配置。 |

### 4.3 UI

| 资产 | 父类 | 职责 |
| --- | --- | --- |
| `WBP_WW_HUD` | `UserWidget` | 主界面容器。 |
| `WBP_WW_SeatList` | `UserWidget` | 展示所有座位。 |
| `WBP_WW_SeatCard` | `UserWidget` | 展示玩家名、存活、票数、AI 思考状态。 |
| `WBP_WW_PhaseBanner` | `UserWidget` | 展示阶段、天数和倒计时。 |
| `WBP_WW_ChatLog` | `UserWidget` | 公共发言、系统广播、私密反馈。 |
| `WBP_WW_ActionPanel` | `UserWidget` | 人类玩家的发言、投票和夜间行动入口。 |
| `WBP_WW_DebugPanel` | `UserWidget` | 展示 AI 请求、prompt、raw output 和解析结果。 |

## 5. 数据结构

建议先在蓝图中创建这些 Enum 和 Struct。

### 5.1 Enum

| 名称 | 值 |
| --- | --- |
| `EWWPhase` | `Boot`, `Lobby`, `AssignRoles`, `NightStart`, `NightWerewolves`, `NightSeer`, `NightWitch`, `Dawn`, `DayDiscussion`, `DayVote`, `Exile`, `CheckVictory`, `GameOver` |
| `EWWRole` | `Werewolf`, `Seer`, `Witch`, `Villager` |
| `EWWTeam` | `WerewolfTeam`, `GoodTeam`, `Neutral` |
| `EWWAliveState` | `Alive`, `KilledAtNight`, `Exiled`, `Dead` |
| `EWWMessageVisibility` | `Public`, `PrivateToPlayer`, `PrivateToWerewolves`, `DebugOnly` |
| `EWWAIRequestType` | `NightAction`, `DaySpeech`, `Vote`, `MemorySummary` |

### 5.2 Struct

| 名称 | 字段建议 |
| --- | --- |
| `FWWPlayerPublicState` | `SeatId`, `DisplayName`, `AliveState`, `bIsHuman`, `LastVoteTargetSeatId`, `PublicClaims`, `SuspicionScore` |
| `FWWPlayerPrivateState` | `Role`, `Team`, `KnownWerewolfSeatIds`, `SeerKnownResults`, `WitchHasAntidote`, `WitchHasPoison`, `PrivateMemory` |
| `FWWChatLine` | `SpeakerSeatId`, `SpeakerName`, `Text`, `Phase`, `DayNumber`, `Visibility`, `Timestamp` |
| `FWWAIDecision` | `RequestType`, `ActorSeatId`, `TargetSeatId`, `SpeechText`, `Reason`, `Confidence`, `RawText`, `bParsedOk` |
| `FWWVoteRecord` | `VoterSeatId`, `TargetSeatId`, `Reason` |
| `FWWNightResult` | `KilledSeatId`, `SavedSeatId`, `PoisonedSeatId`, `SeerCheckedSeatId`, `SeerResultTeam` |
| `FWWPromptContext` | `SystemPrompt`, `PrivatePrompt`, `PublicStateText`, `RecentTranscriptText`, `TaskInstruction`, `OutputSchemaText` |

关键原则：公开状态和私密状态必须分开。Prompt Builder 只能给对应 AI 能合法知道的信息。

## 6. LiteRT-LM 蓝图接入

### 6.1 模型加载

推荐在 `BP_WW_GameMode::BeginPlay` 或未来的 `BP_WW_GameInstance` 中加载一次模型：

```text
Event BeginPlay
  -> Resolve LiteRT-LM Model Path
  -> Load LiteRT-LM Model
     bUseAutoConfig = true
     Backend = "gpu"
     MaxNumTokens = 2048 到 4096
     bEnableStreaming = true
```

模型文件名以 `Content/Models/` 中实际文件为准。

### 6.2 SessionOwner

每个 AI 玩家必须使用独立 Session：

- 使用 `LiteRtLmComponent` 时，组件内部以自身作为 session key。
- 使用 `Send LiteRT-LM Chat Request` 节点时，`SessionOwner` 传对应的 `BP_WW_PlayerAgent`。

不要所有 AI 都把 `SessionOwner` 指向同一个 `GameMode`，否则 AI 玩家会串记忆。

### 6.3 AI 请求队列

本地推理先串行执行：

```text
BP_WW_AIRequestQueue
  PendingRequests: Array<FWWAIRequest>
  bBusy: bool

EnqueueRequest
  Add PendingRequests
  TryStartNext

TryStartNext
  if bBusy return
  if no request return
  bBusy = true
  BuildPrompt
  Send LiteRT-LM Chat Request

OnChunk
  Append to CurrentStreamingText
  Broadcast Debug Update

OnDone
  Parse result into FWWAIDecision
  Return decision to BP_WW_RoundManager
  bBusy = false
  TryStartNext
```

### 6.4 采样建议

| 场景 | Temperature | MaxTokens | 说明 |
| --- | ---: | ---: | --- |
| 夜间行动 | 0.2 - 0.4 | 96 - 160 | 需要稳定、合法。 |
| 白天发言 | 0.7 - 0.9 | 160 - 320 | 需要更像真实玩家。 |
| 投票 | 0.3 - 0.5 | 96 - 160 | 需要理由明确。 |
| 记忆摘要 | 0.2 | 160 - 256 | 只压缩事实。 |

结构化输出建议使用 JSON 约束。若蓝图端解析 JSON 不方便，可以补一个很薄的 Blueprint Function Library，只做 JSON 到 `FWWAIDecision` 的解析；玩法和状态机仍保持蓝图优先。

## 7. Prompt 模板

### 7.1 夜间行动

```text
你是狼人杀玩家 P{SeatId}，角色是 {RoleName}。
当前阶段：{PhaseName}，第 {DayNumber} 夜。

你的私密信息：
{PrivateInfo}

当前公开信息：
{PublicState}

任务：
根据你的角色执行本阶段可用行动。你只能选择仍然存活的合法目标。
不要解释规则，不要输出多余文本。

输出 JSON：
{
  "action": "kill|check|save|poison|skip",
  "targetSeatId": 0,
  "reason": "一句话说明原因",
  "confidence": 0.0
}
```

### 7.2 白天发言

```text
你是狼人杀玩家 P{SeatId}，角色是 {RoleName}。
你需要在白天公开发言。其他玩家不知道你的真实身份，除非你主动跳身份。

公开信息：
{PublicState}

你的私密记忆：
{PrivateMemory}

最近发言：
{RecentTranscript}

输出 JSON：
{
  "speech": "你要公开说出的话，第一人称，80字以内",
  "claimRole": "none|seer|witch|villager",
  "accuseSeatId": 0,
  "defendSeatId": 0,
  "reason": "你的真实策略说明，仅用于调试"
}
```

### 7.3 投票

```text
当前进入投票阶段。你必须从存活玩家中选择一名投票目标，不能投自己。

公开信息：
{PublicState}

最近讨论：
{RecentTranscript}

你的私密目标：
{PrivateGoal}

输出 JSON：
{
  "voteTargetSeatId": 0,
  "publicReason": "公开投票理由，40字以内",
  "privateReason": "真实理由，仅调试"
}
```

## 8. 状态机

`BP_WW_RoundManager` 是唯一修改玩法状态的对象。

```text
StartNewGame
  Spawn PlayerAgents
  Shuffle Roles
  Assign PrivateState
  SetPhase NightStart

NightStart
  DayNumber += 1
  Clear NightResult
  SetPhase NightWerewolves

NightWerewolves
  Ask alive werewolves for kill target
  Resolve target
  SetPhase NightSeer

NightSeer
  Ask alive seer for check target
  Store private result
  SetPhase NightWitch

NightWitch
  Ask alive witch for save or skip
  Apply final night result
  SetPhase Dawn

Dawn
  Broadcast night result
  CheckVictory
  SetPhase DayDiscussion

DayDiscussion
  Each alive player speaks in seat order
  SetPhase DayVote

DayVote
  Collect votes
  Exile highest voted player
  SetPhase CheckVictory

CheckVictory
  if game over SetPhase GameOver
  else SetPhase NightStart
```

非法 AI 输出处理：

1. 记录 raw output。
2. 尝试解析合法目标。
3. 失败时使用默认合法目标或 skip。
4. Debug 面板显示 `bParsedOk = false`。

## 9. UI 页面规划

`WBP_WW_HUD` 推荐布局：

```text
Top:    PhaseBanner
Left:   SeatList
Center: ChatLog
Right:  ActionPanel
Bottom: Human input / Continue button
Overlay: DebugPanel
```

UI 必须清楚区分：

- 公开发言。
- 系统消息。
- 只给人类玩家看的私密夜间反馈。
- 只给开发者看的 Debug 信息。

## 10. UMG MCP 可用后的任务拆分

后续不要一次性让 UMG MCP 做完整游戏。建议按资产拆分：

1. 创建 `WBP_WW_HUD` 主布局。
2. 创建 `WBP_WW_SeatCard` 和 `WBP_WW_SeatList`。
3. 创建 `WBP_WW_ChatLog` 和 `WBP_WW_ChatLine`。
4. 创建 `WBP_WW_ActionPanel`。
5. 创建 `WBP_WW_DebugPanel`。
6. 绑定 RoundManager 事件。
7. 接入 AIRequestQueue 调试输出。

每一步都要能 PIE 验证，不要等所有 UI 做完才测试。

## 11. 里程碑

| 阶段 | 目标 | 验收 |
| --- | --- | --- |
| M0 | 项目 Demo 文档与网页 | `DemoDocs/AIWerewolfDemo/index.html` 可打开。 |
| M1 | 无 AI 规则骨架 | Print String 能跑完整夜晚、白天、投票循环。 |
| M2 | 玩家 Agent 和 HUD | 能显示 6 个座位和公开日志。 |
| M3 | AI 白天发言 | AI 按座位顺序串行发言。 |
| M4 | 夜间行动和投票 | 一局能从开始跑到胜负。 |
| M5 | Debug 教学化 | 能看到 prompt、raw output、解析结果和性能数据。 |
| M6 | UMG MCP 自动化 UI | 用 MCP 生成和调整 Widget 资产。 |

## 12. 测试清单

- 模型能加载，失败时 UI 有错误提示。
- 6 名玩家生成，角色数量正确。
- 狼人知道同伴，好人不知道狼人。
- 预言家查验结果只给预言家。
- 女巫药水不会重复使用。
- AI 发言进入公共日志。
- AI 投票目标合法。
- 放逐后玩家不再参与行动。
- 胜利条件能结束游戏。
- AI 输出空文本或非法 JSON 时不会卡死。
- 重开一局时所有 AI Session 被释放。
- 长局时 prompt 不无限增长。

## 13. 下一步执行建议

下一步应先做 `BP_WW_RoundManager` 和基础 Enum / Struct，用假玩家跑通规则状态机。等规则骨架稳定后，再接 LiteRT-LM 请求队列和 UMG。
