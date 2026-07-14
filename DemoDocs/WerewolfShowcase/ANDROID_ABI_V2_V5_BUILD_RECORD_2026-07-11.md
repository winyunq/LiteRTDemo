# LiteRTDemo ABI v2 / v5 构建与验证记录（2026-07-11）

## Android Shipping native / Gradle debug 测试包

- 本地归档：`D:\UE5Project\LiteRTDemo\Packaged\AndroidV6_SingleAPK\LiteRTDemo-Android-Shipping-arm64.apk`
- Google Drive 根目录副本：`G:\我的云端硬盘\LiteRTDemo-Werewolf-ABI2-GPU-v5-arm64.apk`
- Package：`com.winyunq.litertdemo`
- versionCode：`5`
- versionName：`5.0.0`
- ABI：仅 `arm64-v8a`
- minSdkVersion：`26`
- targetSdkVersion：`34`
- 文件大小：`2,718,236,107` 字节
- APK SHA-256：`49283A0FCE4B5D36872DF4FB0589B815F570A8FC95D9726DFFBF4AFB58C98FD0`
- 外部 OBB：`0`
- APK Signature Scheme v2：`true`
- Unreal native target：`Shipping`
- Gradle variant：`assembleDebug`
- Manifest：`application-debuggable`
- 签名证书：`CN=Android Debug, O=Android, C=US`
- 证书 SHA-256：`20FC9608C35C266FA3B6296CB6B1BDD9C617457AAD310CAB7BD8AD44227F8282`

两个路径对应同一个已校验测试件。该 APK 的 Unreal C++/Blueprint 目标是 Shipping，但 Android 最终封装实际使用 `assembleDebug`，Manifest 也标记为 debuggable，并由 Android Debug 证书签名。它可用于用户侧载与诊断，不是应用商店 Distribution/release 包。

## 内置模型与单 APK 结构

- 源模型：`Content/Models/gemma-4-E2B-it.litertlm`
- 源模型长度：`2,583,085,056` 字节
- 源模型 SHA-256：`AB7838CDFC8F77E54D8CA45EADCEB20452D9F01E4BFADE03E5DCE27911B27E42`
- APK 内模型分片：`4`
- APK 内分片总长度：`2,583,085,056` 字节
- APK 内分片重组结果与源模型 SHA-256 一致。
- APK 包含 `assets/main.obb.png`，但它是 APK 内部游戏载荷；没有需要另外复制的外部 OBB。

因此用户只需复制并安装一个 APK。首次启动仍需要足够的应用持久存储空间来重组模型。

## ABI v2 wrapper 证据

- 包内文件：`lib/arm64-v8a/liblitert_lm_wrapper.so`
- 包内 stripped wrapper 长度：`23,805,672` 字节
- 包内 stripped wrapper SHA-256：`3290CE296E0576CFF25761027912D5275FAC850C027350A188EA9089CE42E9DB`
- 源 wrapper GNU Build ID：`ee77f3a0d0abb2bac51d1e7b6fa9f6b5`
- 包内 stripped wrapper GNU Build ID：`ee77f3a0d0abb2bac51d1e7b6fa9f6b5`
- 源文件与包内文件的 Build ID 一致，说明 APK 中的 stripped 文件来自本次 ABI v2 wrapper 构建。

包内 wrapper 保留了 ABI v2 要求的 7 个导出：

- `LiteRtLm_GetApiVersion`
- `LiteRtLm_GetBuildInfo`
- `LiteRtLm_ResetConversation`
- `LiteRtLm_GetConversationHistorySize`
- `LiteRtLm_GetConversationGeneration`
- `LiteRtLm_GetResolvedBackend`
- `LiteRtLm_GetLoadedToolsCount`

这些是静态 ABI/身份验证证据。它们证明目标 APK 带有 ABI v2 wrapper，不等同于已经在手机上完成 Engine 创建、GPU 推理或 Conversation reset 运行验证。

## Manifest 与诊断路径

Manifest 未申请以下广泛存储权限：

- `READ_EXTERNAL_STORAGE`
- `WRITE_EXTERNAL_STORAGE`
- `MANAGE_EXTERNAL_STORAGE`

插件以 app-scoped `Saved/Diagnostics` 作为主日志，并在 Android 10+ 通过 MediaStore 尝试镜像到手机可见目录：

```text
Download/LiteRTDemo/LiteRTDemo-diagnostics-<session>.jsonl
```

只有真机 RuntimeStatus 显示公共导出健康，且该文件可以实际读取时，才能确认本次手机上的公共日志镜像成功。

## 已完成的主机侧校验

- 单一 ARM64 APK，package/version/SDK 元数据符合配置。
- 没有外部 OBB。
- APK Signature Scheme v2 验证成功，证书指纹已记录。
- 4 个模型分片的数量、总长度和重组 SHA-256 与源模型一致。
- 包内 stripped wrapper 的 SHA-256、Build ID 和 ABI v2 导出已核对。
- Manifest 不包含 `READ_EXTERNAL_STORAGE`、`WRITE_EXTERNAL_STORAGE` 或 `MANAGE_EXTERNAL_STORAGE`。

## 尚未完成的真机验收

截至本记录创建时，用户尚未对本包完成真机交互测试。因此本记录不声称以下项目已经在 Android 手机上验证：

- 首次启动的模型重组、Engine 创建和模型加载；
- 真实 GPU 推理、设备利用率或逐算子 accelerator 落点；
- RuntimeStatus 中 configured/resolved backend、tools count、Engine/Conversation generation 和 reset code 的现场值；
- 狼人/预言家夜间头像点击、投票与目标合法性；
- 输入框提前编辑、轮到玩家后发送，以及完整多昼夜循环；
- `Download/LiteRTDemo` JSONL 公共镜像与崩溃日志取回；
- 前后台切换、低内存恢复和长时间稳定性。

用户完成测试后，应保留最新 JSONL；若发生 native 崩溃，再同时取得 `adb logcat -b crash`。只有结合这些真机证据，才能对 Android GPU 运行和完整玩法作出结论。

## 历史版本

[2026-07-10 V6/version 4 构建记录](ANDROID_V6_BUILD_RECORD_2026-07-10.md)对应 ABI v1 测试包，仅作为历史基线保留。它的大小、哈希和 ABI 结论不能用于标识本次 version 5/ABI v2 APK。
