# OpenShell Mobile 适配规划

本文档用于在正式开发 Android / iOS 版本前统一边界和路线。目标是新增移动端能力，同时尽量不影响现有桌面端体验。

## 目标定位

移动端不做完整桌面版复刻，而是定位为“服务器应急工具”。

优先场景：

- 查看和管理常用连接。
- 快速 SSH 登录服务器。
- 查看 CPU、内存、磁盘、网络等基础状态。
- 执行常用命令。
- 简单上传、下载、查看远程文件。

非目标：

- 复刻桌面双栏 SFTP 工作台。
- 复刻桌面多面板、多 Tab 重度运维体验。
- 长时间复杂终端编辑。
- 后台长期保持 SSH 会话。

## 总体策略

采用同仓库适配，不单开新项目。

核心原则：

- 复用 C++ 核心逻辑：连接配置、SSH、SFTP、终端模型、翻译、设置。
- 新增移动端 QML：手机端使用独立入口和独立页面结构。
- 桌面端 UI 不做移动化改造，现有 `MainWindow.qml` 继续服务桌面端。
- 平台差异集中在 platform 层，不在业务代码里散落大量宏。

最终推荐结构：

```text
src/
  core/
    ConnectionCatalog.*
    SessionController.*
    SettingsStore.*
    ssh/
    terminal/
  desktop/
    TrayController.*
    DesktopWindowServices.*
  mobile/
    MobileFilePicker.*
    MobileSecureStore.*
  platform/
    PlatformServices.h
    DesktopPlatformServices.cpp
    AndroidPlatformServices.cpp
    IosPlatformServices.cpp

qml/
  common/
    ThemedButton.qml
    ThemedTextField.qml
    ThemedTextArea.qml
  desktop/
    MainWindow.qml
    Sidebar.qml
    SessionTabs.qml
    FileBrowser.qml
  mobile/
    MobileWindow.qml
    MobileConnectionsPage.qml
    MobileConnectionEditorPage.qml
    MobileTerminalPage.qml
    MobileSystemPage.qml
    MobileFilesPage.qml
```

迁移节奏不要一步到位。当前项目已有桌面 QML 和 C++ 文件直接位于 `qml/`、`src/` 根目录，短期内不建议大搬家，避免引入路径、资源和 CMake 风险。

第一阶段只新增移动端目录：

```text
qml/
  MainWindow.qml
  Sidebar.qml
  SessionTabs.qml
  FileBrowser.qml
  mobile/
    MobileWindow.qml
    MobileConnectionsPage.qml
    MobileTerminalPage.qml

src/
  AppController.*
  TrayController.*
  platform/
    PlatformServices.h
  mobile/
    MobileFilePicker.*
```

第二阶段在移动端空壳跑通后，再逐步整理：

- 把桌面专用 QML 移入 `qml/desktop/`。
- 把两端共用控件移入 `qml/common/`。
- 把桌面专用 C++ 移入 `src/desktop/`。
- 把平台无关核心移入 `src/core/`。
- 每次移动文件都同步更新 `CMakeLists.txt` 和 `qt_add_qml_module`。

## 平台入口

桌面端继续加载现有入口：

```cpp
engine.loadFromModule("OpenShell", "Main");
```

移动端加载独立入口：

```cpp
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
engine.loadFromModule("OpenShell", "MobileMain");
#else
engine.loadFromModule("OpenShell", "Main");
#endif
```

## 必要平台隔离

### 系统托盘

移动端无系统托盘。

处理方式：

- 桌面端保留 `TrayController`。
- Android / iOS 不创建托盘对象。
- `AppController` 对 QML 暴露的 show / hide / quit 行为在移动端降级为前后台生命周期行为。

### 文件选择

桌面端继续使用 `chooseLocalFile` / `chooseLocalFolder` / `chooseDownloadFolder`
（同步返回路径，背后是 `QFileDialog`）。

移动端走一套独立的异步 API：

- `pickMobileFileAsync()` → 通过 `QFileDialog::getOpenFileContent` 触发系统
  picker（Android SAF / iOS UIDocumentPicker），把选中文件落盘到
  `QStandardPaths::TempLocation`，再通过 `mobileFilePicked(path, error)` 信号
  把可用路径回 QML。
- 文件夹上传当前在移动端是显式 "not yet supported"：SAF 给的是 tree URI，
  iOS 给的是 security-scoped tree bookmark，都需要专门的目录走查器才能复用
  `SftpTransfer::uploadPathRecursive`，独立工程。
- 下载目录：移动端固定落到 `AppDataLocation`，不弹 picker。
- 私钥文件应仅保存导入副本，不假设任意路径长期可读。

### 安全存储

`CredentialStore` 已经做了平台分发：

- 桌面：QtKeychain（`src/CredentialStore.cpp`，宏 `OPENSHELL_USE_KEYCHAIN`）。
- iOS：`src/CredentialStoreIos.mm`，调 Sec API
  (`kSecClassGenericPassword` + `kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly`)。
- Android：`src/CredentialStoreAndroid.cpp` 是 **占位实现** —— base64 写
  app 私有 `QSettings`，沙箱保护但未在静态层面加密。正式做法是
  `AndroidKeyStore` 生成 AES key + `Cipher` GCM 包装；可经 QJniObject 直接调
  Java 标准库或引入 AndroidX Security `EncryptedSharedPreferences`。文件里有
  FIXME 标注，是独立后续工程。

### 后台行为

移动端后台保活受限，尤其 iOS。

策略：

- 前台连接优先。
- 应用进入后台时明确提示会话可能断开。
- 不把后台常驻作为第一期目标。

## 移动端 UI 方案

手机端使用单栏导航，不使用桌面 SplitView。

建议页面：

- 连接列表页：搜索、分组、快速连接、新建/编辑。
- 连接编辑页：全屏表单或 Sheet。
- 终端页：全屏终端、软键盘工具栏、常用按键。
- 系统状态页：卡片式展示 CPU / 内存 / 磁盘 / 网络。
- 文件页：单栏远程文件浏览，后续再加本地文件入口。

终端页需要单独设计移动输入：

- Ctrl、Esc、Tab、方向键工具栏。
- 粘贴按钮。
- 常用命令快捷按钮。
- 长按选择 / 复制策略。

## 阶段计划

### 阶段 0：可行性验证

- 安装 Qt Android 套件、JDK、Android SDK、NDK。
- 让当前工程在 Android toolchain 下完成 CMake 配置。
- 验证 libssh2 / libvterm 可交叉编译。
- 新增空白 `MobileMain.qml` 并在 Android 上启动。

完成标准：

- Android 模拟器或真机能打开空白 OpenShell Mobile 窗口。
- 桌面端构建和运行不受影响。

### 阶段 1：平台隔离

- 抽出 `PlatformServices`。
- 屏蔽移动端系统托盘。
- CMake 按平台加入移动 QML。
- `main.cpp` 按平台加载入口。

完成标准：

- 桌面端仍加载原 `Main.qml`。
- Android 加载 `MobileMain.qml`。
- `AppController` 在无托盘平台不崩溃。

### 阶段 2：连接管理

- 移动端连接列表页。
- 移动端连接编辑页。
- 复用 `ConnectionCatalog` 数据格式。
- 验证新增、编辑、删除连接。

完成标准：

- Android 上可管理连接 Profile。
- 桌面端可读取同样的数据格式。

### 阶段 3：SSH 终端

- 移动端终端页。
- 接入现有 `SessionController`。
- 适配软键盘和常用控制键。
- 处理屏幕旋转、resize。

完成标准：

- Android 真机可连接 SSH。
- 可输入命令、查看输出、断开连接。

### 阶段 4：系统状态

- 复用现有监控采集逻辑。
- 移动端做卡片式状态页。
- 异常和超时提示移动端友好化。

完成标准：

- 可查看 CPU、内存、磁盘、网络基本状态。

### 阶段 5：移动端文件能力

- 先做远程单栏 SFTP 浏览。
- 支持下载到系统选择的位置。
- 支持从系统文件选择器上传。
- 后续再考虑远程文件打开 / 编辑回传。

完成标准：

- 可浏览远程目录。
- 可上传、下载单个文件。

## 依赖准备

Android：

- Qt Android 套件，例如 `android_arm64_v8a`。
- JDK，版本需匹配 Qt 当前版本要求。
- Android SDK / Command-line Tools。
- Android NDK，使用 Qt 官方推荐版本。
- CMake / Ninja。
- Android 模拟器或真机。

iOS：

- macOS。
- Xcode。
- Qt macOS host 套件。
- Qt iOS 套件。
- 模拟器构建可先使用 ad-hoc 签名。
- 真机安装、TestFlight、App Store 发布需要 Apple Developer 签名配置。

## Windows 环境实测记录

记录时间：2026-05-19。

当前机器已有：

- Qt `E:\Qt\6.11.1\msvc2022_64`
- Qt `E:\Qt\6.11.1\mingw_64`
- Qt `E:\Qt\6.11.1\android_arm64_v8a`
- Qt `E:\Qt\6.11.1\android_armv7`
- Qt `E:\Qt\6.11.1\android_x86`
- Qt `E:\Qt\6.11.1\android_x86_64`
- JDK 17：`D:\Program Files\Java\jdk-17.0.11`
- JDK 21：`D:\Program Files\Java\jdk-21.0.1`
- Android SDK：`E:\android\sdk`
- Android platform-tools：已找到 `adb.exe`
- Android cmdline-tools：已找到 `sdkmanager.bat`
- Android platforms：已安装 `android-24`、`android-28`、`android-29`、`android-31`、`android-33`、`android-34`、`android-35`、`android-36`
- Android NDK：已安装 `23.1.7779620`、`25.1.8937393`、`27.0.12077973`、`28.2.13676358`
- Android CMake：已安装 `3.22.1`
- Qt Maintenance Tool：`E:\Qt\MaintenanceTool.exe`

当前注意事项：

- 默认 `JAVA_HOME` / `PATH` 仍指向 `D:\Program Files\Java\jdk1.8.0_361`，需要在 Android 构建脚本或系统环境中切到 JDK 17/21。
- Android 已不再阻塞于 OpenSSL：当前项目在 Android 下使用 bundled mbedTLS 作为 libssh2 加密后端。

已验证：

- 临时设置 `JAVA_HOME=D:\Program Files\Java\jdk-21.0.1` 后，`sdkmanager.bat --list_installed` 可正常列出已安装 Android SDK 包。
- JDK 17 和 JDK 21 均可运行，后续优先使用 JDK 21。
- 使用 `E:\Qt\6.11.1\android_arm64_v8a\bin\qt-cmake.bat`、`-G Ninja`、NDK `27.0.12077973`、`ANDROID_ABI=arm64-v8a` 可启动 Android 配置。
- Android 已接入 bundled mbedTLS，libssh2 可使用 `CRYPTO_BACKEND=mbedTLS` 完成配置。
- Android arm64 APK 已生成：`build-android-arm64/android-build/OpenShell.apk`。

下一步环境动作：

- 设置 `JAVA_HOME` 指向 JDK 21，或在 Android 构建脚本中临时设置。
- 当前 Android 已选择 mbedTLS：项目在 Android 下通过 FetchContent 拉取 mbedTLS v3.6.2，并让 libssh2 使用 `CRYPTO_BACKEND=mbedTLS`。
- 后续需要在真机上验证 libssh2 + mbedTLS 的 SSH 连接兼容性。

已验证的 Android 配置命令：

```powershell
$env:JAVA_HOME='D:\Program Files\Java\jdk-21.0.1'
$env:ANDROID_SDK_ROOT='E:\android\sdk'
$env:ANDROID_NDK_ROOT='E:\android\sdk\ndk\27.0.12077973'
$env:PATH="$env:JAVA_HOME\bin;E:\Qt\Tools\CMake_64\bin;E:\Qt\Tools\Ninja;$env:PATH"

E:\Qt\6.11.1\android_arm64_v8a\bin\qt-cmake.bat `
  -S . `
  -B build-android-arm64 `
  -G Ninja `
  -DANDROID_SDK_ROOT=E:\android\sdk `
  -DANDROID_NDK_ROOT=E:\android\sdk\ndk\27.0.12077973 `
  -DANDROID_ABI=arm64-v8a `
  -DANDROID_PLATFORM=android-35
```

Android 构建命令：

```powershell
$env:JAVA_HOME='D:\Program Files\Java\jdk-21.0.1'
$env:ANDROID_SDK_ROOT='E:\android\sdk'
$env:ANDROID_NDK_ROOT='E:\android\sdk\ndk\27.0.12077973'
$env:PATH="$env:JAVA_HOME\bin;E:\Qt\Tools\CMake_64\bin;E:\Qt\Tools\Ninja;$env:PATH"

E:\Qt\Tools\CMake_64\bin\cmake.exe --build build-android-arm64 --config Debug
```

项目根目录已提供 Android arm64 脚本：

```bat
configure-android-arm64.bat
build-android-arm64.bat
install-android-arm64.bat
run-android-arm64.bat
```

脚本会优先使用 `D:\Program Files\Java\jdk-21.0.1`，再兜底到 `D:\Program Files\Java\jdk-17.0.11`，并默认使用 `E:\Qt\6.11.1\android_arm64_v8a`、`E:\android\sdk`、NDK `27.0.12077973`。

常用方式：

```bat
build-android-arm64.bat
```

有真机或模拟器连接时：

```bat
run-android-arm64.bat
```

## iOS CI 策略

当前阶段先在 GitHub Actions `macos-15` runner 上验证 iOS 模拟器构建，不直接产出可上架或可真机安装的 `.ipa`。

CI 做法：

- 安装 Xcode runner 自带的 iOS Simulator SDK。
- 使用 aqt 安装 Qt macOS host 套件和 Qt iOS 套件。
- 通过 `ios/bin/qt-cmake` 生成 Xcode 工程。
- 使用 `iphonesimulator`、`arm64` 构建。
- `CODE_SIGNING_ALLOWED=NO` 完成构建后，对 `.app` 做临时 ad-hoc codesign。
- 上传 `OpenShell-ios-simulator-arm64.zip` 作为 release artifact。

这个产物适合验证 iOS 编译链、QML 资源、移动端入口和模拟器运行。后续如果要真机安装或发布，需要补：

- Bundle ID 管理。
- Apple Developer Team ID。
- 证书和 provisioning profile。
- `.ipa` 导出配置。
- Keychain / UIDocumentPicker / 后台限制等 iOS 原生能力。

## 风险点

- libssh2 / libvterm 在 Android / iOS 下交叉编译失败。
- 移动端软键盘和终端控制键体验差。
- iOS 后台限制导致长连接不稳定。
- 文件系统沙盒导致 SFTP 上传下载体验需要重设计。
- 密码和私钥安全存储需要平台原生适配。
- 桌面和移动 QML 共享过度会互相拖累，应控制共享范围。

## 开发约定

- 新增移动 QML 放入 `qml/mobile/`。
- 桌面 QML 不做移动端兼容性改造，除非是通用 bug 修复。
- 平台宏优先集中在 `main.cpp`、CMake 和 `src/platform/`。
- 核心层不要直接依赖 Android / iOS 原生 API。
- 新增用户可见字符串后更新翻译文件。

## 第一批任务清单

- [x] 新增 `MobileMain.qml` 和 `qml/mobile/MobileWindow.qml`。
- [x] 调整 `main.cpp`，移动端加载 `MobileMain`。
- [x] 调整 CMake，按平台加入移动 QML。
- [x] 屏蔽移动端 `TrayController`。
- [x] 验证 Android CMake 配置。
- [x] 生成 Android arm64 Debug APK。
- [x] 新增 Android arm64 配置、构建、安装脚本。
- [x] 记录 Android 构建命令和环境版本。
- [x] 验证 Android 模拟器窗口启动，无启动闪退。
- [x] 新增 iOS Simulator CI 构建和临时 ad-hoc 签名产物。
- [ ] 验证 Android 真机 / 模拟器核心页面交互。
- [ ] 验证 iOS Simulator 窗口启动与核心页面交互。
