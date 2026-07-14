# LiteRTDemo V6 构建与验证记录（2026-07-10）

> **历史记录**：本页对应 versionCode 4 / versionName 4.0.0 的 ABI v1 测试包。当前测试件请查看 [2026-07-11 ABI v2 / version 5 构建记录](ANDROID_ABI_V2_V5_BUILD_RECORD_2026-07-11.md)。以下数值保留用于追溯，不代表当前 APK。

## Android Shipping 测试包

- APK：`Packaged/AndroidV6_SingleAPK/LiteRTDemo-Android-Shipping-arm64.apk`
- Package：`com.winyunq.litertdemo`
- versionCode：`4`
- versionName：`4.0.0`
- 文件大小：`2,718,211,867` 字节
- SHA-256：`D6F1354D34175CD874D19674AE48E23BADFA0EF67A70A17E2630CEEAF07B90E6`
- 外部 OBB：`0`
- APK Signature Scheme v2：`true`
- 签名证书：`CN=Android Debug, O=Android, C=US`
- 证书 SHA-256：`20FC9608C35C266FA3B6296CB6B1BDD9C617457AAD310CAB7BD8AD44227F8282`

本包是可安装的真机测试包，不是商店 release 签名包。正式发布必须换用 release keystore 后重新执行全部校验。

## 内置模型校验

- 源模型：`Content/Models/gemma-4-E2B-it.litertlm`
- 源模型长度：`2,583,085,056` 字节
- APK 内模型分片：`4`
- APK 内分片总长度：`2,583,085,056` 字节
- 内置 `.parts`、磁盘 `.parts`、各分片长度和源模型总长度一致。
- APK 同时包含 `assets/main.obb.png`；它是 APK 内部游戏载荷，不是外部 OBB。

## 构建命令

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\Scripts\Package-AndroidSingleApk.ps1 `
  -Configuration Shipping
```

脚本按固定顺序完成 Android Target Build、分片验证、受限清理、Cook、Stage、Pak、Package、Archive，以及 APK 元数据、签名和模型字节验证。UAT 最终 ExitCode 为 0。

## Windows 侧功能证据

- `Saved/Logs/WWGpuStatelessSoak.log`：一次模型加载下 100/100 Stateless native callbacks 完成；每次成功 reset，配置 backend 为 gpu，无 CPU fallback/no-done。
- `Saved/Logs/WWV6UIIntegration7.log`：最终 UI integration 保存成功，`Success - 0 error(s)`。
- `Saved/Logs/WWV6FullVerifyClean.log`：独立重开资产，14/14 检查通过，`Success - 0 error(s)`。
- `Saved/Logs/WWV6RosterSmoke50c.log`：50 人、16 狼；未加载模型时 R1/R2 失败后进入 PhaseCode 5，AliveCount 仍为 50。
- `Saved/Logs/WWV6DefaultMapGpuLoad.log`：默认地图自动以 `Backend=gpu` 加载项目模型并成功创建 engine，无 Blueprint Runtime Error。

## 尚需真机确认

- 首次启动的 APK 内模型重组和所需磁盘空间。
- Android 设备上的严格 GPU 模型加载、发言、角色行动、投票和多昼夜循环。
- App 前后台切换与低内存恢复。
- 本记录对应当时的 ABI v1 包，`Result.Backend` 只记录 configured backend。ABI v2 后续包还必须保存 native `ResolvedBackend`、Engine/Conversation generation、tools count 和 reset code；即便 `ResolvedBackend=gpu`，设备名、利用率和逐算子 accelerator 遥测仍未提供。

打包后检查时 `adb devices -l` 没有列出在线设备，因此本记录不声称已经完成本次 APK 的真机安装或 Android GPU 运行验证。
