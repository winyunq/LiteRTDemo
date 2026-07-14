# Android 单 APK 打包流程（UE 5.8）

这个流程用于生成可直接复制到手机安装的单个 ARM64 APK。LiteRT-LM 模型随 APK 内置，不需要再分发外部 OBB。

固定入口是：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Scripts\Package-AndroidSingleApk.ps1 `
  -Configuration Shipping `
  -ArchiveDirectory .\Packaged\Release-v5.0.0\Android
```

脚本遇到任何错误都会立即停止；只有构建与全部校验都通过时，才会输出绿色的成功信息。

## 环境要求

- Unreal Engine 5.8；本机默认探测 `D:\UE_5.8`。
- Android SDK，包含可用的 `build-tools`、Android platforms 和 NDK r27c。
- NDK 必须为 `27.2.12479018`，且 `source.properties` 中必须同时标记 `Pkg.ReleaseName = r27c`。
- 可用 JDK；默认优先当前 `JAVA_HOME`，其次是 Android Studio 的 `jbr`。
- 模型必须存在：`Content\Models\gemma-4-E2B-it.litertlm`。
- `Config\DefaultEngine.ini` 必须保持 `bPackageDataInsideApk=True`、仅构建 ARM64，并配置正确的包名与版本号。

当前验证环境的 SDK 位于 `D:\SDK`。如果安装位置不同，可以显式传入：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Scripts\Package-AndroidSingleApk.ps1 `
  -EngineRoot "D:\UE_5.8" `
  -AndroidSdkRoot "D:\SDK" `
  -JavaHome "C:\Program Files\Android\Android Studio\jbr"
```

默认构建 `Development`。需要 Shipping 时使用 `-Configuration Shipping`，并先在 UE Android 项目设置中配置正式签名；脚本验证的是签名结构和 v2 签名是否有效，不会替你创建或修改发行证书。

## 不要改变的打包顺序

脚本固化了本轮已经验证成功的顺序：

1. 检查项目、UE 5.8、Android SDK、NDK r27c、JDK、模型文件和单 APK 配置。
2. 临时为当前 PowerShell 进程设置 `ANDROID_HOME`、`ANDROID_SDK_ROOT`、`NDKROOT`、`NDK_ROOT` 和 `JAVA_HOME`。
3. 脚本直接流式读取模型，生成 `Build\Android\LiteRtLmModelChunks` 分片和包含 SHA-1 的清单；不需要项目 `Source` 或项目 `Build.cs`。
4. 检查磁盘分片数量、每片长度、总字节数、清单 SHA-1 与原模型是否一致。
5. 只清理工作区内允许的 Android 旧产物。
6. 通过 RunUAT 的 `BuildCookRun -build -skipbuildeditor` 执行 Android ARM64 Shipping Build、ASTC Cook、Stage、Pak、Package 和 Archive。
7. LiteRT-LM APL 把分片和 Android GPU/runtime `.so` 复制到 APK；Gradle 只生成一个包含主 OBB 载荷与模型分片的 APK。
8. 对最终 APK 做结构、元数据、签名和内置模型完整性检查。
9. 无论成功还是失败，都恢复进入脚本前的进程环境变量。

项目根目录没有 `Source`，仍是 Blueprint-only 项目。插件描述文件显式设置 `EnabledByDefault=false`，项目再通过 `.uproject` 启用插件；这个组合会让 UE 为纯蓝图项目在 `Intermediate\Source` 自动生成临时 Target，并把 C++ 插件静态链接进 Android `libUnreal.so`。不要恢复项目 C++ 模块，也不要移除 `-skipbuildeditor`：Cook 使用已经存在的 Editor 二进制，Android Runtime 则由临时 Target 单独编译。

## 安全清理范围

脚本只允许递归删除以下工作区路径：

- `Saved\Cooked\Android_ASTC`
- `Saved\StagedBuilds\Android_ASTC`
- `Intermediate\Android`
- 本次归档目录；本页发布构建显式使用 `Packaged\Release-v5.0.0\Android`，未传参时脚本的兼容默认值仍为 `Packaged\AndroidV6_SingleAPK`

归档目录必须位于项目的 `Packaged` 子目录下。脚本会拒绝工作区外路径和目录联接/符号链接，不会清理 UE 安装目录、Android SDK/NDK、用户 Gradle 缓存或其他项目，也不会调用 `setx` 修改全局环境。

## 分片与 APK 内部结构

模型不能作为一个超过 2 GB 的单独 Android asset 直接交给 Gradle/aapt。因此 Android Build 会把模型无损切成多个分片：

```text
Build/Android/LiteRtLmModelChunks/
  gemma-4-E2B-it.litertlm.part000.png
  gemma-4-E2B-it.litertlm.part001.png
  ...
  gemma-4-E2B-it.litertlm.parts
```

`.png` 只是避免 Android 打包链对大资源采用不合适处理的容器扩展名，内容仍是原模型的连续原始字节，没有转码或替换。`.parts` 有三行：分片数量、原模型总字节数和完整模型 SHA-1。

最终 APK 必须包含：

```text
assets/main.obb.png
assets/litertlm/gemma-4-E2B-it.litertlm.parts
assets/litertlm/gemma-4-E2B-it.litertlm.part000.png
assets/litertlm/gemma-4-E2B-it.litertlm.part001.png
...
```

`assets/main.obb.png` 是 UE 放在 APK 内部的主游戏载荷，不是需要用户另外复制的外部 OBB。首次启动时，插件会流式读取 `assets/litertlm`，按顺序拼装到应用持久目录，并同时校验总长度和 SHA-1；只有校验通过的原生可读文件才会交给严格 GPU loader。缓存以模型 SHA-1 隔离，同大小的新模型不会误用旧缓存。

## 自动校验项

UAT 结束后，脚本会逐项确认：

- 归档目录递归范围内恰好只有一个 `.apk`，并且没有任何外部 `.obb`。
- APK 大于原模型，避免误把未包含模型的小包当成交付包。
- `aapt2 dump badging` 得到的包名、`versionCode`、`versionName` 与 `DefaultEngine.ini` 完全一致。
- `apksigner verify --verbose --print-certs` 成功，且 APK Signature Scheme v2 为 `true`。
- 使用 `tar -tf` 明确列出 `assets/main.obb.png`、`.parts` 和每一个 `.litertlm.partNNN.png`。
- APK 内 `assets/litertlm` 不多也不少，只包含当前清单要求的分片和 manifest。
- 通过 ZIP64 元数据读取每个内置分片的未压缩长度，并与 Android Build 生成的对应分片逐一比较。
- APK 内分片字节总和、内置 manifest、磁盘 manifest、原模型文件长度和 SHA-1 完全一致。

任一项不满足都会以非零结果停止，不应交付该 APK。

## 真机 RuntimeStatus 与诊断日志

ABI v2 包应在开始游戏前显示 configured `gpu`、native `ResolvedBackend`、Engine/Conversation generation、tools count、最近 reset 与硬件遥测不可用状态。它们的证据边界见 [ABI v2 Runtime/Blueprint 教程](LITERTLM_ABI_V2_RUNTIME_BLUEPRINT_TUTORIAL_zh.md)。

完整上下文诊断默认关闭。需要复现问题时，在设置页勾选“记录详细对话（调试）”；插件才会在应用的 persistent download/external-files 目录写入：

```text
LiteRTLM/litertlm-<session>.jsonl
```

不需要 `READ_EXTERNAL_STORAGE`、`WRITE_EXTERNAL_STORAGE` 或 `MANAGE_EXTERNAL_STORAGE`。实际路径以 `RuntimeStatus.DiagnosticsPath` 为准，可用 `adb pull` 取回。取消勾选后不再复制完整上下文，待写队列被清空，写盘线程停止。完整 native 崩溃栈仍需额外取得 `adb logcat -b crash`。

## 输出与安装

默认成功产物：

```text
Packaged\Release-v5.0.0\Android\LiteRTDemo-Android-Shipping-arm64.apk
```

2026-07-14 本机 Blueprint-only Shipping 构建已通过主机侧校验：versionCode 5 / versionName 5.0.0，package `com.winyunq.litertdemo`，仅含 ARM64，minSdkVersion 26 / targetSdkVersion 34。APK 大小为 `2,713,466,811` 字节，SHA-256 为 `E7A87FF4DB7889E085F68447F21F88DCD5687F1BC6943AB0FE717075AF18B5AD`。APK Signature Scheme v2 为 `true`，内置 4 个模型分片合计 `2,583,085,056` 字节，外部 OBB 数量为 0。

包内已逐项确认 `libUnreal.so`、`liblitert_lm_wrapper.so`、Gemma constraint provider、LiteRT GPU/OpenCL/WebGPU accelerator 与 Top-K sampler。Android Runtime 由本轮临时 Target 使用 NDK r27c 实际编译和链接，不是沿用引擎通用 `UnrealGame` 或旧项目二进制。

本机构建的 Unreal native target 是 Shipping，但 Gradle 最终执行 `assembleDebug`，Manifest 标记为 `application-debuggable`，并使用 `CN=Android Debug, O=Android, C=US` 调试证书（证书 SHA-256：`20FC9608C35C266FA3B6296CB6B1BDD9C617457AAD310CAB7BD8AD44227F8282`）。它可以直接安装做真机测试和诊断，但不是商店 Distribution/release 包。正式发布前必须启用 Distribution、配置自己的 release keystore 后重新打包，并重新记录大小、哈希、Manifest 与证书。交付测试时只复制本次验证通过的 APK。首次启动还会在应用持久目录拼装一份约 2.58 GB 的模型；手机建议预留 10–12 GB 空间，并允许文件管理器“安装未知应用”。

当前可复制测试件位于：

```text
D:\UE5Project\LiteRTDemo\Packaged\Release-v5.0.0\Android\LiteRTDemo-Android-Shipping-arm64.apk
```

[2026-07-11 ABI v2/v5 构建记录](ANDROID_ABI_V2_V5_BUILD_RECORD_2026-07-11.md)与 [2026-07-10 version 4/ABI v1 记录](ANDROID_V6_BUILD_RECORD_2026-07-10.md)仅作为历史基线保留；当前交付以本页 2026-07-14 的路径、大小和哈希为准。

手机已连接电脑时，也可以在项目根目录执行：

```powershell
& "D:\SDK\platform-tools\adb.exe" install -r -g ".\Packaged\Release-v5.0.0\Android\LiteRTDemo-Android-Shipping-arm64.apk"
```

同包名且同签名的旧版本会被 5.0.0 更新替换；若旧版使用了不同签名，Android 会要求先卸载旧版。用户当前选择的是复制后自行安装，因此打包流程不会自动操作手机。

部分 OEM 文件管理器不支持超大 APK。如果出现“无法解析安装包”，先确认脚本所有校验已通过，再使用同一个 APK 通过 ADB 安装；不要把模型改回外部 OBB，也不要删除 APK 内的分片。

上述结果证明 APK 在主机侧的结构与内容；手机上的 GPU 利用率、首次模型重组、OEM 安装兼容性与长局内存表现仍应在目标设备上继续验证。
