# AI 狼人杀 Demo Android 打包记录

本文档属于 `LiteRTDemo` 项目 Demo 文档，不属于插件 API 文档。

## 当前产物

截至 2026-07-07，已在本机生成单 APK：

```text
D:\UE5Project\LiteRTDemo\Saved\AndroidBuilds\AIWerewolf\LiteRTDemo-arm64.apk
```

包名和平台信息：

```text
package: com.YourCompany.LiteRTDemo
minSdkVersion: 26
targetSdkVersion: 34
native-code: arm64-v8a
```

该 APK 已启用 `bPackageDataInsideApk=True`，游戏数据被打进 APK 内部，不需要额外安装 OBB。

## 本机环境

已验证环境：

```text
UE 5.8: D:\UE_5.8
Android SDK: D:\SDK
NDK: D:\SDK\ndk\27.2.12479018
Java: C:\Program Files\Android\Android Studio\jbr
```

Gradle 在当前网络环境下需要强制 IPv4：

```powershell
$env:JAVA_TOOL_OPTIONS='-Djava.net.preferIPv4Stack=true -Djava.net.preferIPv4Addresses=true -Dhttps.protocols=TLSv1.2'
$env:GRADLE_OPTS='-Djava.net.preferIPv4Stack=true -Djava.net.preferIPv4Addresses=true -Dhttps.protocols=TLSv1.2'
```

## 私有 Android `.so`

Android LiteRT `.so` 是本地打包产物，不提交到公开仓库。

插件侧路径：

```text
Plugins\LiteRT-LM-Unreal\Source\ThirdParty\LiteRtLm\Binaries\Android\arm64-v8a\
```

该目录已被插件 `.gitignore` 忽略。打包前需要先从 `D:\LiteRT-LM` 生成并同步：

```powershell
& 'D:\LiteRT-LM\Gemini_build_wrapper_android.ps1'
Copy-Item 'D:\LiteRT-LM\ThirdParty\LiteRtLm\Binaries\Android\arm64-v8a\*.so' `
  'D:\UE5Project\LiteRTDemo\Plugins\LiteRT-LM-Unreal\Source\ThirdParty\LiteRtLm\Binaries\Android\arm64-v8a\' `
  -Force
```

`Gemini_build_wrapper_android.ps1` 已使用：

```text
--linkopt=-Wl,-z,max-page-size=16384
```

生成的 `liblitert_lm_wrapper.so` 已验证为 16KB page-size 兼容：

```text
LOAD Align 0x4000
```

## 大模型处理

`Content\Models\gemma-4-E2B-it.litertlm` 约 2.58GB，直接打包会让 Android OBB 超过 2GB 限制。

当前 APK 不打包 `Content\Models`。AI 狼人杀蓝图 Demo 可以在无模型情况下运行确定性自动测试；后续手机端真实 Gemma 推理应改为外部下载、首次启动拷贝或用户选择模型文件。

## E2B 模型下载

插件已提供蓝图异步下载节点：

```text
Download LiteRT-LM Model
Download Gemma 4 E2B LiteRT-LM Model
```

专用 E2B 节点默认下载：

```text
https://huggingface.co/litert-community/gemma-4-E2B-it-litert-lm/resolve/main/gemma-4-E2B-it.litertlm?download=true
```

下载目标：

```text
ProjectPersistentDownloadDir/LiteRTModels/gemma-4-E2B-it.litertlm
```

相关蓝图辅助节点：

```text
Resolve LiteRT-LM Downloaded Model Path
Does LiteRT-LM Downloaded Model Exist
Load LiteRT-LM Downloaded Model
```

`Load LiteRT-LM Project Model` 也已增加回退逻辑：当 `Content/Models/<ModelFileName>` 不存在，而持久下载目录中存在同名模型时，会自动加载下载目录中的模型。

推荐蓝图流程：

```text
Does LiteRT-LM Downloaded Model Exist("gemma-4-E2B-it.litertlm")
  true  -> Load LiteRT-LM Downloaded Model
  false -> Download Gemma 4 E2B LiteRT-LM Model
             OnProgress  -> 更新下载进度 UI
             OnCompleted -> bSuccess 时 Load LiteRT-LM Downloaded Model
```

下载节点支持可选 `ExpectedSha256`。如果填写 64 位十六进制 SHA-256，下载完成后会流式校验文件；校验失败会删除 `.part` 临时文件并返回错误。

## UAT 命令

```powershell
$env:ANDROID_HOME='D:\SDK'
$env:ANDROID_SDK_ROOT='D:\SDK'
$env:NDKROOT='D:\SDK\ndk\27.2.12479018'
$env:NDK_ROOT='D:\SDK\ndk\27.2.12479018'
$env:JAVA_HOME='C:\Program Files\Android\Android Studio\jbr'
$env:JAVA_TOOL_OPTIONS='-Djava.net.preferIPv4Stack=true -Djava.net.preferIPv4Addresses=true -Dhttps.protocols=TLSv1.2'
$env:GRADLE_OPTS='-Djava.net.preferIPv4Stack=true -Djava.net.preferIPv4Addresses=true -Dhttps.protocols=TLSv1.2'

& 'D:\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat' BuildCookRun `
  -project='D:\UE5Project\LiteRTDemo\LiteRTDemo.uproject' `
  -noP4 `
  -platform=Android `
  -clientconfig=Development `
  -build -cook -stage -pak -package -archive `
  -archivedirectory='D:\UE5Project\LiteRTDemo\Saved\AndroidBuilds\AIWerewolf' `
  -map=/Game/AIWerewolf/L_AIWerewolfDemo `
  -utf8output
```

## 验证

已通过项：

- UE Android target platform 已安装。
- UAT `BuildCookRun` 成功。
- `Android VALID r27c`。
- `bPackageDataInsideApk = True`。
- APK 包含 `liblitert_lm_wrapper.so`、`libLiteRt.so` 和 LiteRT 相关 Android 依赖。
- 从 APK 抽出的 `liblitert_lm_wrapper.so` 已验证 `LOAD Align 0x4000`。

当前没有 adb 设备在线，因此尚未完成真机安装运行验证。
