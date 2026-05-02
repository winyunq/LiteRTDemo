# Winyunq LiteRT-LM 项目状态与后续蓝图 (2026-05-01)

## 🏆 当前成就 (Current Progress)

### 1. 核心插件: `LiteRT-LM-Unreal`
- [x] **底层封装**: 完成 Google LiteRT-LM 的 C-API 封装，实现异步流式推理。
- [x] **会话隔离 (The Soul)**: 实现基于 `void* ctx` 的会话映射机制，支持多 Agent 记忆共存。
- [x] **AI 大脑组件**: 完成 `ULiteRtLmComponent`，支持蓝图异步回调与 System Prompt 动态配置。
- [x] **开源透明**: 代码结构清晰，符合虚幻引擎插件标准。

### 2. 品牌与文档
- [x] **Markdown 文档**: 完成《快速开始》、《模型管理》、《API 参考》三部曲。
- [x] **网页版门户**: 在 `Document/index.html` 实现“硬核生产力”风格的交互式文档页面（Tailwind CSS 驱动）。
- [x] **定价策略**: 确立 $19 降维打击方案，并在文档中植入品牌价值。

### 3. Demo 项目: `LiteRTDemo`
- [x] **架构升级**: 项目已从纯蓝图升级为 C++ 项目，实现 Demo 与插件逻辑的物理隔离。
- [x] **高性能 HUD**: 完成 `ADemoTavernHUD`，可实时监测微秒级延迟与显存占用。
- [x] **UMG C++ 驱动**: 完成 `UWinyunqDialogueWidget`，实现全息对话流与 AI 组件的深度绑定。

---

## 📅 明日待办与后续计划 (Roadmap)

### 第一阶段：Demo 关卡视觉润色 (Visual Polish)
- [ ] **场景搭建**: 在 `L_TavernDemo` 关卡中利用 Lumen 布光，营造赛博酒馆氛围。
- [ ] **UI 适配**: 在编辑器中创建 `WBP_Dialogue` 资产，完善 CSS 风格的全息边框效果。
- [ ] **NPC 实例化**: 配置 Ada、Kael 和 Nexus 三个不同性格的 NPC 资产。

### 第二阶段：性能与稳定性压测 (Stress Test)
- [ ] **多 Agent 压力测试**: 同时开启 5 个以上 NPC 对话，观察显存增长曲线。
- [ ] **打包验证**: 确保模型文件在 `Non-UFS` 模式下能正确打包并运行。

### 第三阶段：发布准备 (Launch)
- [ ] **录制 Demo 视频**: 重点录制“注视 NPC 瞬间切换对话”的丝滑感。
- [ ] **完善 README**: 增加 GitHub Stars 引导和 Marketplace 购买入口链接。
- [ ] **撰写发布日志**: 以 Winyunq 名义发布 V1.0 稳定版。

---

## 🛠️ 技术备注 (Tech Notes)
- **核心竞争力**: 我们的 Context Switch 延迟实测应控制在 **800us - 1200us** 之间（RTX 4060）。
- **品牌口号**: 战略由人，战术由 AI (Strategy by Human, Tactics by AI)。

---
*Winyunq Core Engineering - 让本地 AI 推理成为每一款游戏的标配。*
