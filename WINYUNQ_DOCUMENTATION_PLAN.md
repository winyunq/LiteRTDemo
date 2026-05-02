# LiteRT-LM Unreal 开发者文档最终实施战略 (Final Strategy)

## 1. 核心愿景 (Core Vision)
超越源码注释的广度，深挖物理逻辑的深度。建立一套基于“虚幻引擎即循环 (UE5 as a Loop)”哲学、图文并茂、具有极高工程指导价值的技术手册。

## 2. 逻辑架构 (The 7 Pillars - Final)

引导顺序自上而下 (Top-Down Reading)，编写深度自下而上 (Bottom-Up Writing)。

| 章节 | 文件名 | 用户导向语 | 核心物理深度 |
| :--- | :--- | :--- | :--- |
| **1. 关于与背景** | `index.html` | “了解我们，了解 Google LiteRT-LM” | Winyunq 理念、ABI 防火墙设计意图、后端加速技术选型。 |
| **2. 快速入门** | `QUICK_START.html` | “5 分钟内看到第一个 AI 回复” | 环境门槛探测、模型 mmap 加载路径规范、首个推理 Demo。 |
| **3. 应用接口层** | `APP_LAYER.html` | “通过这些 API 迅速上手项目” | `ULiteRtLmComponent` 详尽契约、蓝图事件绑定、人格 Prompt 注入。 |
| **4. 插件工作流** | `SERVICE_LAYER.html` | “更好地了解我们的插件工作流程” | Subsystem 全局调度、会话池 LRU 算法、显存预算分配策略。 |
| **5. 底层核心原理** | `API_LAYER.html` | “获取插件最底层的运行原理” | **全符号拆解**：KV Cache 物理映射、异步推理泵、Token 级约束解码。 |
| **6. 实战 Demo** | `DEMO_CASES.html` | “看看在真实业务中怎么用” | 酒馆对话、战术指令集、多模态输入处理的完整范式。 |
| **7. 打包与发布** | `SHIPPING.md` | “将你的 AI 之魂推向玩家” | 模型分发 (Pak/External)、显存预算配置、各平台发布注意事项。 |

## 3. 文档编写准则 (The Winyunq Standard)

### 3.1 必须包含的“比源码更深”的维度
- **参数契约 (Contract)**：明确 `NULL` 输入的物理后果，谁分配内存，谁负责销毁。
- **物理映射 (Physical Mapping)**：用 SVG 展现每一个 API 调用在显存、CPU 线程中的物理位移。
- **线程模型 (Threading)**：详述底层异步线程与虚幻 GameThread 的数据交换协议。
- **副作用 (Side Effects)**：调用 A 函数后，底层的状态机会发生什么不可逆的变化。

### 3.2 技术选型
- **物理文件**：纯静态 HTML 跳转（`<a>` 标签），拒绝动态加载 MD 产生的 404 和延迟。
- **视觉语言**：Winyunq 工业深色风，代码块高亮，SVG 矢量逻辑图。

## 4. 当前进度与断点
- [x] 第一阶段：制定“游戏即循环”的底层分类逻辑。
- [x] 第二阶段：初步搭建 `INITIALIZATION_CONFIG.html` 与 `API_LAYER.html` 的视觉原型。
- [ ] 下一动作：按照 **“全符号、模块化、带物理图解”** 的标准，补全第 5 章（底层核心原理）的每一个函数详述。
