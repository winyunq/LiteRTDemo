# LiteRTDemo 身份、多样性与隐私修复包构建记录（2026-07-11）

## 当前 Android 测试件

- 本地归档：`D:\UE5Project\LiteRTDemo\Packaged\AndroidV6_Diversity_20260711\LiteRTDemo-Android-Shipping-arm64.apk`
- Google Drive 根目录副本：`G:\我的云端硬盘\LiteRTDemo-Werewolf-ABI2-GPU-v5-DiversityFix-20260711-arm64.apk`
- Package：`com.winyunq.litertdemo`
- versionCode：`5`
- versionName：`5.0.0`
- ABI：仅 `arm64-v8a`
- 文件大小：`2,718,244,663` 字节
- APK SHA-256：`FEAE6BC5A2831D2A29A51788037E089640E91CAC04089A1AE7A297ACDCB90B0A`
- 外部 OBB：`0`
- APK Signature Scheme v2：`true`
- Unreal native target：`Shipping`
- Gradle variant：`assembleDebug`
- Manifest：`application-debuggable`
- 签名：沿用 Android Debug 证书

两个路径对应本轮同一个单体 APK。它可以直接侧载测试，但由于最终 Android 封装仍为 `assembleDebug`、Manifest 可调试且使用 Debug 证书，它不是应用商店 Distribution/release 包。versionCode/versionName 沿用 `5 / 5.0.0`；测试件必须以本记录中的文件名、长度和 SHA-256 区分，不能只看应用内版本号。

## 本包包含的 09:50–09:52 蓝图修复

本 APK 在以下资产保存后构建：

- `BP_WW_GameCore.uasset`：2026-07-11 09:50:46；
- `WBP_WW_MessageRow.uasset`：2026-07-11 09:52:35；
- `WBP_WW_PlayerCard.uasset`：2026-07-11 09:52:36；
- `WBP_WW_MainV5.uasset`：2026-07-11 09:52:55。

因此它取代 00:39 的旧 v5 测试件，并明确包含以下 identity/diversity/privacy 改动：

- 每局打乱 40 个唯一中文姓名和 40 个人格描述，并把姓名、人格、角色、私密记忆和响应证据稳定绑定到同一玩家索引；
- 讨论 prompt 使用该席姓名、人格、本人上次发言和最多 3,200 字符的最新同日公开讨论，要求回应具体观点；完整已提交发言仍保留在玩家可见的公开消息列表中；
- Blueprint 为讨论与目标填入了不同的 `temperature/topP/topK/maxTokens`；后续源码审计确认该历史包的 native wrapper 未消费 temperature/topP/topK，因此不能声称高/低随机度实际生效，只有 MaxTokens 需按独立证据判断；
- 完全重复已有公开发言会以 `duplicate_speech` 拒绝；“我是狼人”“我是今晚的狼人”等明显隐藏身份泄漏会以 `role_leak` 拒绝。拒绝结果不会写入公开消息或推进权威游戏状态；
- 每条已经公开的 AI 发言可以显示通用 `LOCAL GPU AI` 标签和不可逆响应 fingerprint，用于区分实际响应；它不会显示该次请求的完整或缩短 RequestId；
- 实时推理状态只描述“夜间行动、白天发言或投票推理”等阶段/动作类型，不得把当前 GPU inference 关联到某个席位、姓名、RequestId、隐藏角色、人格、私密记忆或候选目标；
- 某席发言提交后，消息行仍正常显示公开的说话人姓名与头像。这是游戏已经公开的信息，与在推理进行中提前暴露“谁正在推理”不同。

## 蓝图主机侧验证

打包前的完整只读验证记录为：

```text
Saved/Logs/WWV6FullDiversityVerify-20260711.log
```

该验证得到 `18/18 PASS`、`0 error`，其中与本轮修复直接相关的结果包括：

- Core、Main、Message、Card、GameMode 和 Settings 均为 `BS_UP_TO_DATE`；
- 姓名池 `40`、人格池 `40`、角色标签 `5`；
- 讨论和目标动作在 Blueprint 中配置了不同采样字段，但 temperature/topP/topK 未被当时 wrapper 消费；
- Stateless 请求通过 RequestId、configured GPU 和 context reset 三重门控；
- 公开 AI 证据为 `fingerprint=visible generic_label=true request_seat_private=hidden`；
- UMG 组件树中 Canvas Panel 数量为 `0`；
- Android 默认地图、GameMode、ARM64、单 APK、包名和 SDK 配置符合要求。

这些结果证明生成并保存的蓝图结构包含上述数据流和隐私门控；它们不替代真机上的模型输出与 GPU 执行证据。

## 角色袋与完整流程复核

构建后又对当前 `BP_WW_GameCore` 做了一次不加载模型、不保存资产的运行时规则探针：

```text
Saved/Logs/WWV6FullFlowAfterRestore-20260711.log
```

该次 10 人随机角色码为 `[1,0,4,3,0,0,1,0,2,1]`，按公开稳定映射 `0=村民、1=狼人、2=预言家、3=守卫、4=女巫` 统计为 `4 村民 + 3 狼人 + 预言家/守卫/女巫各 1`。探针连续完成了 3 个夜晚、3 轮讨论和 3 轮投票，并以好人阵营胜利结束；夜间技能、非法目标、错误阶段提交和玩家非当前回合预输入门控均通过。

因此旧画面中多名角色重复说“我是今晚的狼人”不代表角色数组被分配成了全员狼人。旧会话的 31 次请求拥有独立 RequestId 且都经过 GPU/native 完成，但响应 fingerprint 全部相同；这是模型输出坍缩。本历史包曾尝试通过随机姓名/人格、同日公开讨论上下文、Blueprint 采样字段以及 `duplicate_speech` / `role_leak` 拒绝处理它；后续审计证明 temperature/topP/topK 并未在 native wrapper 生效，且当前主线已允许正常跳身份/诈身份并改用 `private_context_echo`。本记录只描述旧包，不能代表当前行为。

测试工具首次生成隔离 soak 资产时还发现并修复了一个 Python 导入副作用：`build_showcase_v6_core.py` 现有明确的 `__main__` 守卫，作为辅助模块导入时不会重建生产 Core。修复后生成器只写 `/Game/WerewolfShowcase/Tests/`，并已用 Core、Main、GameMode、`DefaultEngine.ini`、`DefaultGame.ini` 的生成前后 SHA-256 不变证明生产资产未被再次触碰。恢复后的正式资产再次通过：

```text
Saved/Logs/WWV6FullDiversityVerify-AfterSoakGuard-20260711.log
WW_V6_FULL_VERIFY SUMMARY: PASS checks=18 failures=0
```

这些构建后的源工程复核不改变上方 APK 的字节内容和 SHA-256；APK 没有因测试工具修复而重新打包。

## 单 APK 与内置模型

- APK 内模型分片：`4`；
- 分片总长度：`2,583,085,056` 字节；
- 外部 OBB：`0`；
- 游戏载荷与模型分片均位于同一个 APK 内，用户不需要另外复制 OBB 或模型文件。

打包流程已经完成单 APK 数量、package/version、ARM64、APK Signature Scheme v2、内置 manifest、模型分片数量及逐分片长度检查。完整打包顺序和安装方式见 [Android 单 APK 打包流程](ANDROID_SINGLE_APK_zh.md)。

## 真机证据边界

本包构建和复制时，`adb devices -l` 没有列出已授权设备。因此本记录不声称以下事项已经在 Android 手机上通过：

- 首次启动时模型分片重组、Engine 创建和模型加载；
- 真实 GPU 推理、设备利用率或逐算子 accelerator 落点；
- 随机姓名/人格在多局中的实际显示，以及 AI 发言是否在现场保持有意义的差异；
- 狼人、预言家、守卫和女巫夜间点击，公开发言、投票与多昼夜胜负流程；
- 推理中状态是否始终不泄漏当前行动席位或隐藏角色；
- `Download/LiteRTDemo` JSONL 公共镜像、暂停原因和 native 崩溃日志取回；
- 前后台切换、低内存恢复和长时间稳定性。

用户真机测试时应以本记录的 SHA-256 识别 APK，并保留最新 `Download/LiteRTDemo/LiteRTDemo-diagnostics-<session>.jsonl`。若发生 native 崩溃，再同时取得 `adb logcat -b crash`。只有结合这些现场证据，才能声称 Android 严格 GPU 推理和完整玩法已经验证。

Windows 侧已生成隔离的 18 人 GPU 多样性 soak 地图和元数据分析器，但在本记录补充验证时，另一项目 `D:\UE5Project\Winyunq` 的 Unreal Editor 正占用 RTX 4060，显存仅剩约 2.9 GB。为避免影响未保存的编辑器工作或制造显存不足假故障，本轮没有擅自关闭该编辑器，也没有把该 soak 标记为已运行。

## 历史测试件

- [2026-07-11 ABI v2/version 5 初始构建记录](ANDROID_ABI_V2_V5_BUILD_RECORD_2026-07-11.md)对应 00:39 的旧测试包，不包含本轮 09:50–09:52 修复；
- [2026-07-10 V6/version 4 构建记录](ANDROID_V6_BUILD_RECORD_2026-07-10.md)对应 ABI v1，只作为更早的历史基线保留。
