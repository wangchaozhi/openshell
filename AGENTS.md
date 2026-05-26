# OpenShell AI 维护说明

本文档面向 Codex、Claude Code 等代码助手，先读这个再修改项目。

## 项目概览

OpenShell 是 Qt 6 / QML / C++20 的跨平台 SSH / SFTP / Telnet 终端工具，定位类似 FinalShell。

当前状态：**已是可用的客户端**，不再是骨架。已实现：

- 真实 SSH 会话（libssh2 后端），多 Tab、resize、Ctrl+C 中断
- Telnet 终端会话（明文 TCP，基础 IAC 协商、TTYPE、NAWS；可按 `login:` / `Username:` / `Password:` 提示自动填充用户名/密码；仅终端，不支持 SFTP/监控/跳板机/转发）
- 终端渲染：libvterm 屏幕模型经自研 `QQuickPaintedItem` 渲染（光标、颜色、文本属性）
- 终端选区与复制：拖选、全选、双击选词、三击选行、Shift 扩展、选中即复制
- SFTP 双栏文件浏览：本地/远程浏览、排序、上传、下载、删除、重命名、新建、chmod
- 远程目录同步：可选地把远程文件浏览器同步到终端提示符检测出的当前目录
- 远程文件打开 / 编辑回传：内置编辑器或外部编辑器保存后自动上传回原路径
- 服务器监控：CPU / 内存 / 网络
- 系统托盘常驻、关窗不退出
- 跳板机 (ProxyJump)：通过自定义 libssh2 send/recv 回调走 direct-tcpip 通道
- 端口转发：Local (-L) 已实现；Remote (-R) / Dynamic (-D) 显式 `not yet implemented`
- 断线重连：按 `autoReconnect` / `reconnectMaxAttempts` / 指数退避（封顶 30s）
- 多语言：zh_CN、ja_JP、ko_KR、de_DE 完整；en 走源字符串兜底
- 移动端（Android / iOS）：`qml/mobile/` 下另一套 UI；文件选择器走 `QFileDialog::getOpenFileContent` + `mobileFilePicked` 信号；iOS 凭据用 Sec API 真 Keychain，Android 当前是沙箱 QSettings 占位

剩余目标态能力见下方“待办”。

## 常用命令

Windows 本地：

```bat
scripts\run-vs-debug.bat
scripts\deploy-vs-debug.bat
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

macOS / Android / iOS 见 `scripts/configure-mac.sh`、`scripts/build-android.sh`、`scripts/build-ios.sh`。所有平台脚本统一在 `scripts/` 下，根目录保持干净；脚本里 `ROOT_DIR` 用 `$(dirname "${BASH_SOURCE[0]}")/..` / `%~dp0..` 自动回到仓库根。

## 模块职责

C++（`src/`）：

- `AppController`：应用中枢，向 QML 暴露 Q_INVOKABLE。实现按职责拆到多个文件：
  `AppControllerConnections / Sessions / LocalFiles / RemoteFiles / RemoteEdit / Lifecycle.cpp`。
- `ConnectionCatalog`：维护 `ConnectionProfile` 列表，读写 `<AppData>/OpenShell/connections/<id>.json`（`QSaveFile` 原子写入）。
- `CredentialStore`（仅桌面端）：把 `password` / `keyPassphrase` 存进系统钥匙串，
  JSON 文件不再落明文。详见“凭据存储”一节。
- `SessionController`：活动会话列表，按协议创建 SSH/Telnet worker，worker thread 信号回传 `sessionOutput`。
- `SettingsStore`：`QSettings` 封装（语言、窗口几何、最小化到托盘等）。
- `TranslationManager`：运行时切换语言（system / en / zh_CN / ja_JP）。
- `TrayController`：系统托盘菜单。
- `SystemMonitorController`：服务器 CPU / 内存 / 网络监控。
- `terminal/VtScreen`：libvterm 屏幕模型封装；`terminal/TerminalScreenItem`：QML 渲染项。
- `ssh/`：`SshSession`、`SshChannelWorker` / `Libssh2ChannelWorker`（PTY 通道）、
  `TelnetChannelWorker`（基础 Telnet 通道）、
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
    QString password;        // 桌面端/iOS 走钥匙串，不落 JSON；Android 暂落沙箱 QSettings
    QString privateKeyPath;
    QString keyPassphrase;   // 同上
    QString group;
    QString notes;
    int     lastUsedEpoch = 0;
    int     connectTimeoutSec = 10;
    int     keepaliveSec = 30;   // libssh2 keepalive 间隔，<=0 关闭

    bool    autoReconnect = true;           // 非用户主动断开后自动重连
    int     reconnectMaxAttempts = 5;
    int     reconnectInitialDelayMs = 1000; // 指数退避，封顶 30s

    bool    telnetAutoLogin = true;         // Telnet: 检测 login/password 提示后自动发送
    QString telnetTerminalType = "xterm-256color"; // Telnet TTYPE 响应

    // 跳板机；jumpHost 空字符串表示直连。jumpPassword/jumpKeyPassphrase
    // 同样进钥匙串。
    QString jumpHost; int jumpPort = 22;
    QString jumpUsername; QString jumpAuthType;
    QString jumpPassword; QString jumpPrivateKeyPath; QString jumpKeyPassphrase;

    QVector<PortForward> forwards;          // 目前只消费 type == "L"
};
```

存储位置：`QStandardPaths::AppDataLocation/connections/<id>.json`，每个连接一个文件，
便于云同步 / Git 跟踪。

## 凭据存储

`CredentialStore` 是命名空间级 API（`save` / `load` / `remove`），按平台选实现：

- 桌面（`src/CredentialStore.cpp`，`OPENSHELL_USE_KEYCHAIN`）：QtKeychain
  写入系统钥匙串（macOS Keychain / Windows 凭据管理器 / Linux libsecret）。
- iOS（`src/CredentialStoreIos.mm`）：直接调 Sec API
  (`kSecClassGenericPassword` + `kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly`)。
- Android（`src/CredentialStoreAndroid.cpp`，**占位实现**）：base64 写进
  app-private QSettings。**Not encrypted at rest** —— 正经做法是接 AndroidX
  Security 或手写 `AndroidKeyStore` AES-GCM 包装，文件里有 FIXME 说明，是独立后续工程。
- 任何后端下，受保护字段：`password` / `keyPassphrase` / `jumpPassword` /
  `jumpKeyPassphrase`，JSON 里不写。
- 升级兼容：旧 JSON 里残留的明文会在首次加载时被读出并迁移进 keychain，同时把
  文件里的明文字段抹掉（见 `ConnectionCatalog::loadFromFile`）。
- QtKeychain 通过 FetchContent 引入（`0.14.3`）；Linux 构建需 `libsecret-1-dev`。

## 跳板机与端口转发

- 跳板机：`Libssh2ChannelWorker` 在配置了 `jumpHost` 时先连到跳板机做 handshake+auth，
  开 `direct-tcpip` 通道到目标，再通过 `libssh2_session_callback_set2(SEND/RECV)`
  让主 session 的 I/O 走这个通道。`SessionAbstract` 结构统一管理 session abstract
  指针，避免 kbdint 回调和 jump 回调互相覆盖。
- 端口转发：`forwards` 数组里每项 `type == "L"` 会在 worker 线程上启一个 `QTcpServer`，
  每个 accept 出来的 socket 对应一个 direct-tcpip 通道，`pumpForwards()` 在 `pump()`
  循环里双向倒字节。Remote / Dynamic 暂未实现，遇到会显式 `errorOccurred()`。

## 断线重连

- `SshSession` 维护 `m_userRequestedStop` + `m_reconnectAttempt` + 单 `QTimer`。
- 收到 worker 的 `disconnected` 信号时：若不是用户主动停 + `autoReconnect` 开
  + 重试次数未到 `reconnectMaxAttempts`，调度一次按 `reconnectInitialDelayMs <<
  (attempt-1)` 退避（封顶 30s）的重连；否则把状态置为 `disconnected` 并允许线程退出。
- 线程不在 `disconnected` 时自动 `quit()`：现在只在用户主动 stop 后才退线程，保证
  worker 对象在重连之间存活、`m_thread` 可继续接 `start` slot。

## 开发约定

- C++20。
- 新增 C++ 源文件后必须同步 `CMakeLists.txt`。
- 新增 QML 文件后必须加入 `qt_add_qml_module` 的 `QML_FILES`。
- 新增用户可见字符串 → 更新 `translations/OpenShell_zh_CN.ts` 与 `OpenShell_ja_JP.ts`。
- 不要把密码、私钥等敏感内容写进日志。
- 修改 CI 前先读 `.github/workflows/release.yml`。

## 待办（按优先级）

1. **连接文件加密**：保存的 connections JSON 可选 GPG / age 加密以利 Git 同步。
   详见 [docs/connection-encryption-design.md](docs/connection-encryption-design.md)
   —— 设计已写，实现未做（故意推迟，避免做出半生不熟的 crypto）。
2. **Android 凭据加固**：当前 `CredentialStoreAndroid.cpp` 是 base64+sandbox 占位，
   要替换为 `AndroidKeyStore` 包装的 AES-GCM。需要 QJniObject 调用 KeyGenerator /
   KeyStore / Cipher。
3. **移动端文件夹上传**：`chooseLocalFolder` 在移动端目前直接报“not yet supported”。
   做完整 SAF / iOS security-scoped tree bookmark 走器才能恢复批量目录上传。
4. **Remote / Dynamic 端口转发**：当前只实现了 `-L`。`-R` 需要
   `libssh2_channel_forward_listen_ex` + 反向 accept；`-D` 需要 SOCKS5 解析。
5. **Telnet 增强**：当前是基础终端通道，已支持自动登录开关和自定义 TTYPE。后续可做
   per-profile 编码（GBK/Latin-1 等）、CRLF 模式、LINEMODE/CHARSET/NEW-ENVIRON
   等更多协商，以及更可配置的登录提示匹配规则。
6. **继续扩充测试**：已有
   `test_connection_catalog` / `test_vt_screen` / `test_session_controller` /
   `test_sftp_transfer` / `test_sftp_connection_pool`。仍未覆盖：
   `Libssh2ChannelWorker` 的 reconnect 退避计算、`SftpDirectoryLister`。
7. **翻译校对**：继续校对 zh_CN / ja_JP / ko_KR / de_DE 的术语一致性和语气。

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
src/           CredentialStore.cpp (桌面 QtKeychain),
               CredentialStoreIos.mm, CredentialStoreAndroid.cpp
src/terminal/  VtScreen, TerminalScreenItem
src/ssh/       SshSession, SshChannelWorker, Libssh2ChannelWorker,
               TelnetChannelWorker,
               SftpConnectionPool, SftpDirectoryLister, SftpTransfer
qml/           MainWindow, Sidebar, ConnectionEditor (含 jump host + 转发字段),
               ConnectionManagerView, SessionTabs, TerminalView, FileBrowser,
               SystemInfoView, Themed*, mobile/, filebrowser/
translations/  OpenShell_zh_CN.ts, OpenShell_ja_JP.ts,
               OpenShell_ko_KR.ts, OpenShell_de_DE.ts
tests/         test_connection_catalog, test_vt_screen, test_session_controller,
               test_sftp_transfer, test_sftp_connection_pool
               （QTest，OPENSHELL_BUILD_TESTS=ON 启用）
docs/          architecture.md, mobile-adaptation-plan.md,
               connection-encryption-design.md
```
