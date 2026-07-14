# LiteRT-LM 原生会话与工具调用探针

这个测试绕过 Unreal 游戏和蓝图，直接使用 Demo 当前随包 DLL 的稳定接口：

- `LiteRtLm_GetApi`
- `litertlm::LiteRtLm::Create`
- `NewContext`
- `Ask` / `AskJson`
- `ExportMemoryJson` / `ImportMemoryJson`

它不会使用或覆盖 `D:\LiteRT-LM\bazel-bin`，也不会修改 DLL。

## 运行

```powershell
& 'D:\UE5Project\LiteRTDemo\Tests\Native\Run-LiteRtLmConversationProbe.ps1'
```

脚本会：

1. 用 MSVC 编译 `LiteRtLmConversationProbe.cpp`。
2. 直接加载项目插件目录内的 `litert_lm_wrapper.dll` 和配套 GPU sidecar。
3. 加载项目实际使用的 `gemma-4-E2B-it.litertlm`。
4. 强制 GPU 并检查 `FULLY_ACCELERATED` 证据。
5. 在 A/B 两个逻辑 Context 中写入不同代码，交替切换后要求模型复述。
6. 导出 A 的记忆，导入新 Context，再次要求复述。
7. 不提供 `<|tool_call>` 模板，让模型原生调用目标选择工具。
8. 用正确的 Gemma4 tool-result 消息继续对话。
9. 让模型通过工具参数自由生成一段包含“月亮”的发言。

完整的 prompt、文本、`response_json`、请求前后记忆和指标会写入：

```text
Saved/Diagnostics/NativeProbe/LiteRtLmConversationProbe-*.jsonl
```

## 2026-07-13 验证结果

使用 DLL：

```text
D:\UE5Project\LiteRTDemo\Plugins\LiteRT-LM-Unreal\Source\ThirdParty\LiteRtLm\Binaries\Win64\litert_lm_wrapper.dll
SHA256 F1B5B7A5E1C097D7D5AC39C8B94D41F06B6730EE6904C6C81DEC1FCA03B16214
```

最终结果：15 项通过，0 项失败。

- 严格 GPU：通过，NVIDIA GeForce RTX 4060 Laptop GPU，完整加速证据成立。
- A/B 会话隔离：通过。
- 会话切换后记忆复述：通过。
- 记忆导出/导入后复述：通过。
- 原生目标工具调用：通过，结构化参数为 `seat: 7.0`。
- tool-result 回灌：通过。
- MCP 自由发言：通过，模型自行生成了自然中文句子并放入 `speech` 参数。

通过日志：

```text
Saved/Diagnostics/NativeProbe/LiteRtLmConversationProbe-20260713-090046.jsonl
```

## 已定位的 UE 插件问题

底层 DLL 和 C++ facade 能正确完成工具结果回灌，但 UE 插件当前的 `SubmitToolResult` 与 Gemma4 的实际返回不匹配：

1. Gemma4 的原生 `tool_calls` 没有 `id` 字段。
2. `LiteRtLmToolJson::ParseToolCalls` 因此得到空的 `FLiteRtLmToolCall.Id`。
3. `ULiteRtLmAgent::SubmitToolResult` 又拒绝空 `ToolCallId`，所以这个蓝图 API 无法继续。
4. `MakeToolResultMessage` 生成的是 `tool_call_id + content`；本次验证成功的 Gemma4 格式是：

```json
{"role":"tool","content":{"name":"select_probe_target","response":{"accepted":true,"seat":7}}}
```

建议 UE 插件后续提供两种明确操作：

- `ContinueWithToolResult(ToolName, ResultJson, Options)`：提交结果并继续推理。
- `AppendToolResultToMemory(ToolName, ResultJson)`：只闭合工具调用历史，不额外触发推理，适合狼人杀的发言、投票和夜间行动节点。

狼人杀当前在读取工具调用后直接推进游戏，但没有给相同 Agent 的历史追加对应 `role=tool` 结果。启用长期记忆后，这会留下未闭合的 assistant tool call，后续应使用第二种操作补齐。
