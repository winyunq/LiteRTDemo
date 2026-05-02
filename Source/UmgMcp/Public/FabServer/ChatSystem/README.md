# ChatSystem

`ChatSystem` 提供分层对话入口：

- `SendMessage`：与 AI Provider 的原子通信。
- `ChatMessage`：以 `SendMessage` 为基础，承载常规聊天闭环（含 MCP 语义扩展位）。
- `TaskMessage`：以 `ChatMessage` 为原子，注入 Agent/Target 任务上下文。

当前实现先把入口层统一到 `UUmgMcpChatSystemSubsystem`，便于后续继续将 MCP 循环与 Task 协议状态机收敛到同一架构。