# OpenShell AI 维护说明

本文档面向 Codex、Claude Code 等代码助手，先读这个再修改项目。

## 项目概览

OpenShell 是 Qt 6 / QML / C++20 的跨平台 SSH / SFTP 终端工具，定位类似 FinalShell。

当前状态：**已是可用的客户端**，不再是骨架。已实现：

- 真实 SSH 会话（libssh2 后端），多 Tab、resize、Ctrl+C 中断
- 终端渲染：libvterm 屏幕模型经自研 `QQuickPaintedItem` 渲染（光标、颜色、文本属性）
- 终端选区与复制：拖选、全选、双击选词、三击选行、Shift 扩展、选中即复制
- SFTP 双栏文件浏览：本地/远程浏览、排序、上传、下载、删除、重命名、新建、chmod
- 远程目录同步：可选地把远程文件浏览器同步到终端提示符检测出的当前目录
- 远程文件打开 / 编辑回传：内置编辑器或外部编辑器保存后自动上传回原路径
- 服务器监控：CPU / 内存 / 网络
- 系统托盘常驻、关窗不退出
- 多语言：zh_CN、ja_JP 完整；en 走源字符串兜底
- 移动端（Android / iOS）：`qml/mobile/` 下另一套 UI

剩余目标态能力见下方“待办”。

## 常用命令

Windows 本地：

```bat
run-vs-debug.bat
deploy-vs-debug.bat
```

CMake 直接调：

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build --config Debug
```

单测：

```bash
cmake -S . -B build -DOPENSHELL_BUILD_TESTS=ON -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build
ctest --test-dir build
```

macOS / Android / iOS 见 `configure-mac.sh`、`build-android.sh`、`build-ios.sh`。

## 模块职责

C++（`src/`）：

- `AppController`：应用中枢，向 QML 暴露 Q_INVOKABLE。实现按职责拆到多个文件：
  `AppControllerConnections / Sessions / LocalFiles / RemoteFiles / RemoteEdit / Lifecycle.cpp`。
- `ConnectionCatalog`：维护 `ConnectionProfile` 列表，读写 `<AppData>/OpenShell/connections/<id>.json`（`QSaveFile` 原子写入）。
- `CredentialStore`（仅桌面端）：把 `password` / `keyPassphrase` 存进系统钥匙串，
  JSON 文件不再落明文。详见“凭据存储”一节。
- `SessionController`：活动会话列表，真正的 SSH 流在 worker thread 上跑，信号回传 `sessionOutput`。
- `SettingsStore`：`QSettings` 封装（语言、窗口几何、最小化到托盘等）。
- `TranslationManager`：运行时切换语言（system / en / zh_CN / ja_JP）。
- `TrayController`：系统托盘菜单。
- `SystemMonitorController`：服务器 CPU / 内存 / 网络监控。
- `terminal/VtScreen`：libvterm 屏幕模型封装；`terminal/TerminalScreenItem`：QML 渲染项。
- `ssh/`：`SshSession`、`SshChannelWorker` / `Libssh2ChannelWorker`（PTY 通道）、
  `SftpConnectionPool`、`SftpDirectoryLister`、`SftpTransfer`。

QML（`qml/`）：

- `MainWindow`：主窗口 + SplitView + Sidebar + SessionTabs + TerminalView + FileBrowser。
- `Sidebar` / `ConnectionList` / `ConnectionEditor` / `ConnectionManagerView`：连接管理。
- `TerminalView` / `SessionTabs`：终端会话与多 Tab。
- `FileBrowser` 及 `filebrowser/`：SFTP 双栏文件浏览。左右窗格已拆成
  `filebrowser/LocalPane.qml` / `RemotePane.qml`，`FileBrowser.qml` 只剩状态与逻辑。
- `SystemInfoView`：服务器监控展示。
- `Themed*` / `ThemePalette` / `AccentCard`：主题化控件与配色。
- `mobile/`：移动端独立 UI。

## 数据模型

```cpp
struct ConnectionProfile {
    QString id;          // 自动生成的 UUID
    QString name;
    QString protocol;    // ssh, sftp, telnet
    QString host;
    int     port = 22;
    QString username;
    QString authType;    // password, key, agent
    QString password;        // 桌面端存进钥匙串，不落 JSON；移动端仍在沙箱内
    QString privateKeyPath;
    QString keyPassphrase;   // 同上
    QString group;
    QString notes;
    int     lastUsedEpoch = 0;
    int     connectTimeoutSec = 10;
    int     keepaliveSec = 30;   // libssh2 keepalive 间隔，<=0 关闭
};
```

存储位置：`QStandardPaths::AppDataLocation/connections/<id>.json`，每个连接一个文件，
便于云同步 / Git 跟踪。

## 凭据存储

- 桌面端（`OPENSHELL_USE_KEYCHAIN`）：`password` / `keyPassphrase` 经 `CredentialStore`
  存进系统钥匙串（QtKeychain），JSON 文件里不写这两个字段。
- 升级兼容：旧 JSON 里残留的明文会在首次加载时被读出并迁移进钥匙串，同时把文件里的
  明文字段抹掉（见 `ConnectionCatalog::loadFromFile`）。
- 移动端不接 QtKeychain，凭据仍写在各自 App 私有沙箱内的 JSON。
- QtKeychain 通过 FetchContent 引入（`0.14.3`）；Linux 构建需 `libsecret-1-dev`。

## 开发约定

- C++20。
- 新增 C++ 源文件后必须同步 `CMakeLists.txt`。
- 新增 QML 文件后必须加入 `qt_add_qml_module` 的 `QML_FILES`。
- 新增用户可见字符串 → 更新 `translations/OpenShell_zh_CN.ts` 与 `OpenShell_ja_JP.ts`。
- 不要把密码、私钥等敏感内容写进日志。
- 修改 CI 前先读 `.github/workflows/release.yml`。

## 待办（按优先级）

1. **继续扩充测试**：已有 `test_connection_catalog` / `test_vt_screen` /
   `test_session_controller`；`SftpConnectionPool`、`SftpTransfer` 等仍未覆盖。
2. **跳板机 / 端口转发 / 断线重连**：在 SSH 协议层累加。
3. **加密 / 同步**：保存的 connections 文件可选 GPG / age 加密，方便 Git 同步。

> 大文件拆分已完成：`FileBrowser.qml` 拆出 `LocalPane` / `RemotePane`；
> `TerminalScreenItem.cpp` 拆成核心+绘制 / `*Input.cpp` / `*Selection.cpp`。

> 英文不需要单独的翻译文件：源码字符串即英文，`TranslationManager` 对 `en`
> 故意不装载翻译器。新增字符串只需同步 `zh_CN` / `ja_JP` 的 `.ts`。

## 常见坑

- Windows 上要装 Qt `msvc2022_64`，不要混用 MinGW。
- macOS 上的 Qt 目录是 `Qt/<version>/macos`。
- Linux 上 `xcb-cursor0` / `libxkbcommon-x11-0` 等 X11 依赖要装齐。
- `QtKeychain` 是第三方库（非 Qt 官方），引入时记得更新 CMake 和 CI 缓存键。
- 翻译 `.ts` 后缀和 TypeScript 冲突，IDE 可能误报；不影响 lrelease。

## 当前文件清单速查

```text
src/           AppController(+6 个分文件), ConnectionCatalog, SessionController,
               SettingsStore, TranslationManager, TrayController,
               SystemMonitorController
src/terminal/  VtScreen, TerminalScreenItem
src/ssh/       SshSession, SshChannelWorker, Libssh2ChannelWorker,
               SftpConnectionPool, SftpDirectoryLister, SftpTransfer
qml/           MainWindow, Sidebar, ConnectionEditor, ConnectionManagerView,
               SessionTabs, TerminalView, FileBrowser, SystemInfoView,
               Themed*, mobile/, filebrowser/
tests/         test_connection_catalog, test_session_controller
               （QTest，OPENSHELL_BUILD_TESTS=ON 启用）
```
