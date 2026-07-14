# LiteRT-LM UE5 纯蓝图接入教程：用 Agent 构建本地 AI 狼人杀

这份教程对应当前 `fabLiteRTLMUnreal` 对象式接口和项目内狼人杀 Demo。项目自身没有 `Source` 目录：玩法、角色状态、串行调度、工具校验和 UMG 都在 Blueprint 中；C++ 插件负责共享模型、严格 GPU 推理、Agent 记忆、工具调用解析和诊断日志。

## 1. 先理解运行模型

开发者面对四层对象：

1. `Project Settings > Plugins > LiteRT-LM`：配置模型、上下文、采样与日志；
2. `ULiteRtLmSubsystem`：持有一个共享模型和一个串行 GPU 队列；
3. `ULiteRtLmAgent`：代表一个具有独立系统提示、工具声明和规范记忆的 AI 对象；
4. `ULiteRtLmComponent`：可选的 Actor 包装，会在 `BeginPlay` 自动创建 Agent。

本 Demo 是“一个模型、一个 GPU、多个 Agent、严格串行”，不是为每个玩家复制模型，也不是并行推理：

```text
P1 Agent Ask → 完成并提交节点 → P2 Agent Ask → …… → 阶段结算
```

切换 Agent 时，插件用该 Agent 的规范记忆恢复原生 Conversation。每个玩家因此具有独立聊天历史，但任何时刻只有一个请求占用模型。

## 2. 项目设置与自动加载

把模型放在：

```text
Content/Models/gemma-4-E2B-it.litertlm
```

在 Project Settings 的 LiteRT-LM 页面设置模型路径。当前 Gemma 4 E2B 制品使用：

```text
严格 GPU，不允许 CPU fallback
Max Context Tokens = 32000
Temperature        = 1.0
Top P              = 0.95
Top K              = 64
Random Seed        = true
```

`32000` 来自制品实际 context tensor 边界，不要填写 `32768`、`65536` 或基础模型标称的 `131072`。输入、系统提示、Agent 历史、工具声明和输出共同占用这一个上下文预算。

Blueprint 不需要再放一个 `Initialize Model` 或 `Load Model` 节点。创建首个 Agent 时，Subsystem 自动异步加载共享模型；第一次 `Ask` 也会兜底触发加载。设置页只展示加载/严格 GPU 状态，模型未就绪时禁用开始按钮，玩家不需要手动选择文件或推动加载。

## 3. 每名 AI 创建一个 Agent

开局按 AI 玩家数量调用 `New LiteRT-LM Agent`，保存到与座位同索引的 Agent 数组。不要按消息临时创建 Agent。

每个 `Agent Config` 至少包含：

- `Display Name`：本局随机生成且稳定的公开姓名；
- `System Prompt`：简短的狼人杀玩法说明、该玩家身份和仅其有权知道的初始信息；
- `Tool Declarations Json`：下节的两个工具。

不要添加“推理派、关系派、谨慎派”等人格模板，也不要提供示例发言。Demo 的目的就是展示模型在真实游戏信息上的自由回答。

一个玩家对象的最小生命周期：

```text
开局：New LiteRT-LM Agent
轮到该玩家：Ask（当前任务 + 当下合法选项）
On Completed：读取 Result.ToolCalls
Blueprint：验证并提交行为
节点闭合：Append Tool Result to Memory
公开结果：Append Memory Message 广播给所有有权看见的 Agent
对局结束：Close Agent
```

## 4. 公有信息与私有信息

插件不会猜哪些狼人杀信息应公开，游戏 Blueprint 必须明确分流。

只写给单个/特定 Agent 的私有信息包括：

- 自己的真实身份；
- 狼人队友；
- 预言家查验结果；
- 女巫药物状态和当夜被袭击信息；
- 尚未公开的夜间选择。

应一次性广播给所有存活 Agent 的公开信息包括：

- 主持人宣布的昼夜与死亡结算；
- 每名玩家已提交的公开发言；
- 玩家公开的身份声称、指控和辩护；
- 每张已提交的放逐票，以及谁投给了谁；
- 放逐结果和公开死亡结果。

广播使用：

```text
Append Memory Message(Role, Content)
```

私密结果只追加到有权知道它的 Agent。轮到某 Agent 时，`Ask` 只需发送当前任务和当前合法选项。不要同时把同一份完整公屏历史反复塞进每次输入并再次追加到 Agent 记忆，否则会重复占用上下文。

当前 Demo 仍保留一份完整游戏快照用于 Blueprint 规则校验和诊断；把它构造成模型输入时必须避免与 Agent 已有公开记忆重复。这是长对局内存与上下文排查时的首要检查项。

## 5. 两个原生工具

每个狼人杀 Agent 固定声明两个工具：

```json
[
  {
    "type": "function",
    "function": {
      "name": "submit_werewolf_speech",
      "description": "提交你最终决定公开说出的狼人杀发言",
      "parameters": {
        "type": "object",
        "properties": {
          "speech": { "type": "string" }
        },
        "required": ["speech"],
        "additionalProperties": false
      }
    }
  },
  {
    "type": "function",
    "function": {
      "name": "select_werewolf_target",
      "description": "提交当前狼人杀节点要求的目标席位",
      "parameters": {
        "type": "object",
        "properties": {
          "seat": { "type": "integer" }
        },
        "required": ["seat"],
        "additionalProperties": false
      }
    }
  }
]
```

`Tool Declarations Json` 只是工具声明，不是文字伪装的 MCP。真实调用必须出现在 `FLiteRtLmResult.ToolCalls` 中。Blueprint 使用 `Try Get Tool String Argument` 或 `Try Get Tool Integer Argument` 读取结构化参数；模型正文里写出类似 JSON 的字符串不能通过这条路径。

## 6. 每轮只发送自然任务提示

发言节点：

```text
现在轮到你发言。在确定最终发言后，调用 submit_werewolf_speech 发送你想公开说的内容。
```

普通决策节点：

```text
现在轮到你进行放逐投票。合法目标为 P1、P3、P7。
请根据目前的游戏信息决定你要投给谁，然后调用 select_werewolf_target 提交目标席位。
```

狼人、预言家和守卫只替换动作名称与合法目标。女巫节点明确允许取消：

```text
现在轮到你决定女巫是否用药。
调用 select_werewolf_target 指定用药对象：选择本夜被袭击者表示使用解药，
选择其他合法玩家表示使用毒药，选择 0 表示不使用任何药。
```

不再给模型 `{Mode}`、内部阶段码、性格模板、发言范文或“只返回某个文字 JSON”的额外枷锁。模型自由决定说什么；Host 只要求它通过当前节点对应的真实工具提交最终行为。

## 7. 工具回调、校验与重试

`On Text Chunk` 只用于通用进度或后台诊断，不能把未完成内容显示为公开发言，也不能暴露 thought channel。

`On Completed` 按以下顺序处理：

1. 检查请求确实完成且严格 GPU 证据有效；
2. 要求本节点恰好出现预期工具；
3. 从 `Arguments Json` 读取 `speech` 或 `seat`；
4. Blueprint 验证当前阶段、actor、存活状态、冻结候选、身份权限和技能资源；
5. 只有全部通过才修改权威游戏状态；
6. 调用 `Append Tool Result to Memory` 闭合工具调用；
7. 把已公开结果广播给所有有权看见的 Agent。

女巫 `seat=0` 是显式合法的“取消用药”，不会消耗解药或毒药。其他节点是否允许 `0` 由规则层决定，不能因为参数是整数就直接接受。

没有调用工具、工具名错误、参数非法或目标不合法时：

1. 不修改游戏状态；
2. 保持同一个 actor 和同一个游戏节点；
3. 将拒绝原因作为工具回执/任务提醒写回该 Agent；
4. 再次发送自然提示，要求调用正确工具完成当前节点；
5. 基础设施重试预算耗尽时进入可诊断暂停，绝不生成模板发言、默认投 P1/P2、随机击杀或跳过节点。

## 8. 固定顺序与公开投票

发言和投票都按存活座位的固定顺序串行执行，因为底层模型不能并发。投票不是隐藏批处理：

```text
P1 提交 select_werewolf_target(seat=4)
→ 规则验证
→ 公屏显示“P1 姓名 投给 P4”
→ 广播该票给所有 Agent
→ P2 开始投票并能看到 P1 的公开票型
```

投票开始时冻结合法候选名单，防止同一轮候选边界漂移；但已提交票型属于公屏信息，不能隐藏到最终结算。人类玩家点击高亮玩家卡提交自己的票，UI 同样显示其姓名、头像和目标。

## 9. 发言/投票 thinking 开关

设置页提供两个独立选项，默认关闭：

- `发言时启用思考`：只设置公开发言请求的 `Ask Options.Enable Thinking`；
- `投票时启用思考`：只设置白天放逐投票请求的 `Enable Thinking`。

狼人袭击、预言家查验、守卫守护和女巫用药始终关闭 thinking，以减少移动端等待。插件只在单次请求的 system message 前临时注入 Gemma 官方 `<|think|>` 控制 token，完成后会从 Agent 规范记忆中移除；公开 UI 只展示最终工具提交内容，不展示思维过程。

## 10. UMG 与玩家操作

主界面使用 SafeZone、VerticalBox、HorizontalBox、WrapBox、ScrollBox 和 SizeBox 等自适应容器，不依赖 Canvas 绝对坐标。

开始游戏后，人类玩家只需要：

- 随时在输入框预先编辑自己的发言，轮到自己时点击发送；
- 投票或执行身份技能时点击一张高亮的合法玩家卡。

阶段推进、AI 队列、工具重试、昼夜结算和胜负判断全部由 Host 自动完成。实时状态可以显示“正在进行白天发言/放逐投票/夜间行动”，不能显示具体哪个 Agent 正在推理，以免泄露身份。

## 11. 诊断日志

完整上下文诊断默认关闭，正常游戏不会复制 Agent 完整记忆、维护日志队列或启动写盘线程。设置页勾选“记录详细对话（调试）”时，蓝图调用：

```text
Get LiteRT-LM Runtime → Set Detailed Diagnostics Enabled(true)
```

随后可从 `Get Runtime Status` 取得 `DiagnosticsPath`。Windows 通常位于：

```text
<Project>/Saved/LiteRTLM/litertlm-YYYYMMDD-HHMMSS.jsonl
```

Android 使用应用 persistent download/external-files 目录，实际路径以设置页显示为准。取消勾选会立即停止新记录并清空尚未写入的诊断队列。启用时队列最多 32 MB、按批次 flush；队列溢出只丢诊断记录，不阻塞串行 GPU 推理。开发日志应能关联：

- Agent id/name（仅开发日志，不显示在游戏状态）；
- 该 Agent 实际使用的 `memory_json` 与本次输入；
- 模型最终输出与原生 `ToolCalls`；
- 输入/输出 token、prefill、TTFT、decode 和总耗时；
- 当前/峰值进程内存与错误阶段。

这些信息用于证明每个 AI 看到了哪些公开/私有上下文，以及卡住发生在 GPU 请求、工具解析还是游戏规则校验。它们包含私密游戏信息，只用于开发调试；不要在玩家公屏显示隐藏角色、目标候选、Agent 名称或 thought 内容。

## 12. 人工验收清单

1. 设置页自动加载模型并明确显示严格 GPU；失败时没有 CPU fallback。
2. 至少 10 人开局，每个 AI 座位只创建一个持久 Agent，模型只加载一次。
3. 随机姓名和身份在本局稳定，不包含性格/话术模板。
4. 后发言者能实质看到此前公开发言；私密身份不会广播给无权 Agent。
5. 发言必须产生一次原生 `submit_werewolf_speech`，正文 JSON 不能伪装成工具调用。
6. 投票/夜间节点必须产生一次原生 `select_werewolf_target`，非法目标不会推进游戏。
7. 女巫选择 `0` 能正常跳过且不消耗药物。
8. 每一张票都显示“谁投给谁”，并进入后续 Agent 的公开记忆。
9. 两个 thinking 开关只影响各自请求；夜间技能保持非思考。
10. 长对局观察 context token、内存与耗时，确认没有把同一公屏历史同时重复注入输入和 Agent 记忆。
11. 对模型无工具调用、错误工具、非法参数和推理失败分别测试：当前 actor 不变、没有默认票或模板发言、日志能给出明确阶段。
