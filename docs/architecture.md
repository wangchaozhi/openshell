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
│  AppController ── ConnectionCatalog                      │
│              │── SessionController ── (SSH workers)      │
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
4. `SessionController::open` 当前只是把一条 stub session 加入列表，发 `sessionsChanged`。
5. `MainWindow` 通过 `Connections` 监听 `sessionsChanged`，刷新 `activeSessions`，`SessionTabs` 和 `TerminalView` 跟着重绘。

## 持久化

- `QSettings`：UI 设置（语言、窗口几何、最小化到托盘）。
- `<AppData>/OpenShell/connections/<uuid>.json`：每个连接一个文件，便于版本控制。
- 当前 **没有** 加密。引入 `QtKeychain` 后建议：
  - `password` 字段不写 JSON，存 keychain；JSON 留 `passwordRef: "openshell/<uuid>"`。
  - `privateKeyPath` 继续放 JSON，私钥本身仍在文件系统里由 OS ACL 保护。

## 后续接入 SSH 的建议路径

```
SessionController
  └─ SshWorker(QObject + QThread)
       ├─ libssh2: ssh_session, channel, sftp_session
       ├─ 接收 PTY 数据 → emit sessionOutput(sessionId, bytes)
       └─ 接收 QML 输入 → channel_write
```

`TerminalView` 暂时是 `TextArea`，等接入后换成：

- 自研：`QQuickPaintedItem` + 简化 vt100 解析（颜色、光标、滚屏、resize）。
- 或：嵌入 `QTermWidget`（Qt Widgets 风格，但成熟）。

## 翻译

- `qt_add_translations` 把 `OpenShell_zh_CN.ts` 编译成 `qm`，资源前缀 `:/i18n`。
- `TranslationManager::installLanguage` 根据 `system` / `en` / `zh_CN` 装载对应 `qt_xx` + `OpenShell_xx`。
- 新增字符串记得手动同步 `.ts`，或者 `lupdate` 后再补译。

## 单元测试

- `OPENSHELL_BUILD_TESTS=ON` 才会生成 `test_connection_catalog`。
- 测试用 `QTemporaryDir` + `QStandardPaths::setTestModeEnabled(true)` 隔离 AppData。
- 覆盖：必填校验、UUID 自动分配、字段往返、删除。
