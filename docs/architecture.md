# OpenShell 架构概览

## 进程结构

```
┌─────────────────────────────────────────────────────────┐
│                       QApplication                       │
│  ┌──────────────────────────────────────────────────┐    │
│  │              QQmlApplicationEngine                │    │
│  │   ┌──────────────────────────────────────────┐    │    │
│  │   │           Main.qml → MainWindow          │    │    │
│  │   │   Sidebar │ SessionTabs │ TerminalView   │    │    │
│  │   └──────────────────────────────────────────┘    │    │
│  └──────────────────────────────────────────────────┘    │
│                                                          │
│  AppController ── ConnectionCatalog ── CredentialStore   │
│              │── SessionController ── (SSH workers)      │
│              │── SystemMonitorController                 │
│              │── SettingsStore                           │
│              │── TranslationManager                      │
│              └── TrayController                          │
└─────────────────────────────────────────────────────────┘
```

`AppController` 作为 QML 与 C++ 之间的桥，用 `Q_PROPERTY` 暴露当前连接、语言、最小化到托盘等设置，用 `Q_INVOKABLE` 暴露 `connectionProfiles()` / `saveConnectionProfile()` / `openSession()` 等动作。

## 数据流

1. 启动：`main.cpp` 创建 `AppController` 注入 QML 上下文，加载 `OpenShell::Main`。
2. `AppController` 构造时初始化所有 controller，并把 catalog 列表推给 tray。
3. 用户在 `Sidebar` 双击连接 → `MainWindow.qml` 调 `appController.openSession(id)`。
4. `SessionController::open` 调 worker factory 造一个 `Libssh2ChannelWorker`，
   交给新 `SshSession`，session 拉起 worker QThread 跑 `start()`。
5. worker `connected` / `output` 信号经 queued connection 回到 GUI 线程，
   `SshSession` 喂给 `VtScreen`，再由 `screenUpdated` 通知 `TerminalView` 重绘。
6. 断线时：若 `profile.autoReconnect`，`SshSession` 按指数退避调度一次重连；
   否则状态置为 `disconnected`，线程退出。

## 持久化

- `QSettings`：UI 设置（语言、窗口几何、最小化到托盘）。
- `<AppData>/OpenShell/connections/<uuid>.json`：每个连接一个文件，便于版本控制。
- 凭据：`password` / `keyPassphrase` / `jumpPassword` / `jumpKeyPassphrase` 都进 OS
  钥匙串，JSON 里不写。`CredentialStore` 按平台分实现：
  - 桌面：`src/CredentialStore.cpp`（QtKeychain，宏 `OPENSHELL_USE_KEYCHAIN`）。
  - iOS：`src/CredentialStoreIos.mm`（Sec API，
    `kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly`）。
  - Android：`src/CredentialStoreAndroid.cpp`（**沙箱占位**：base64 写
    app-private `QSettings`，未在静态层面加密；FIXME 标注后续要换 AndroidKeyStore
    + AES-GCM）。
- JSON 文件本身的可选加密（GPG / age 包装）尚未实现，设计草案见
  `docs/connection-encryption-design.md`。

## SSH 层

```
SessionController
  └─ SshSession (GUI 线程)
       │      ↕ queued signals
       └─ SshChannelWorker / Libssh2ChannelWorker (worker QThread)
            ├─ libssh2 session + PTY channel
            ├─ ProxyJump 时：jump session + direct-tcpip 隧道 + 自定义 SEND/RECV 回调
            ├─ Local 端口转发：QTcpServer + 每连接一个 direct-tcpip 通道
            └─ pump() 循环：input/output/forward 通道一并轮询
```

- **跳板机**：`profile.jumpHost` 非空时，worker 先连跳板机做 handshake/auth，开
  `direct-tcpip` 通道到目标，然后用 `libssh2_session_callback_set2(SEND/RECV)`
  让主 session 的 I/O 走该通道。`SessionAbstract` 结构同时承载 jump tunnel
  指针和 kbdint 临时密码，避免二者互相覆盖 abstract 槽。
- **端口转发**：`profile.forwards` 数组里 `type == "L"` 的条目，worker 在自己线程
  上启 `QTcpServer`；每个 accept 创建一个 direct-tcpip 通道，`pumpForwards()` 在
  主 pump 循环里非阻塞地把字节在 socket / channel 之间倒。`-R` / `-D` 暂未实现，
  显式报 `errorOccurred`。
- **断线重连**：见 `SshSession::handleDisconnected` —— 配合
  `m_userRequestedStop` 区分手动停止与意外掉线；重连定时器按
  `reconnectInitialDelayMs << (attempt-1)` 退避，封顶 30 秒。

## 翻译

- `qt_add_translations` 把 `OpenShell_*.ts` 编译成 `qm`，资源前缀 `:/i18n`。
- `TranslationManager::installLanguage` 装载 `qt_xx` + `OpenShell_xx`。
  当前识别：`zh_CN` / `ja_JP` / `ko_KR` / `de_DE` / `en`。
- `ko_KR.ts` / `de_DE.ts` 目前是空骨架，等 `lupdate -ts <file>` 把源串灌进去
  之后再人工翻。

## 单元测试

`OPENSHELL_BUILD_TESTS=ON` 启用以下 QTest target：

- `test_connection_catalog` —— 校验、UUID 分配、字段往返、删除。
- `test_vt_screen` —— libvterm 屏幕模型 / 滚屏 / 选区。
- `test_session_controller` —— 用 `EchoChannelWorker` 跑端到端会话流转。
  注意：fixture 内 `makeProfile` 把 `autoReconnect` 关掉，因为这些用例断言的是
  终态而不是重连循环。
- `test_sftp_transfer` —— 纯函数：`joinRemotePath` / `permissionsToOctal` /
  `localPathSize`（含临时目录树聚合）。
- `test_sftp_connection_pool` —— `ensureLibssh2` 幂等、`Lease` 移动语义、
  lane 隔离、释放后复用。

测试用 `QTemporaryDir` + `QStandardPaths::setTestModeEnabled(true)` 隔离 AppData。
