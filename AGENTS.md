# OpenShell AI 维护说明

本文档面向 Codex、Claude Code 等代码助手，先读这个再修改项目。

## 项目概览

OpenShell 是 Qt 6 / QML / C++20 的跨平台 SSH / SFTP 终端工具骨架，定位类似 FinalShell。当前状态：**仅骨架**——窗口、连接列表、托盘、翻译、配置面板能跑，SSH 协议与终端渲染未接入。

核心能力（目标态）：

- 多 Tab 终端会话
- SFTP 双栏文件浏览
- 服务器监控（CPU/Mem/网络）
- 保存的连接 Profile（密码 / 私钥 / Agent）
- 系统托盘常驻、关窗不退出
- 多语言（默认 zh_CN + en，可拓展）

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
cmake --build build --target test_connection_catalog
ctest --test-dir build
```

## 模块职责

- `AppController`：应用中枢，连接 SettingsStore / Catalog / Sessions / Tray / 翻译，向 QML 暴露 Q_INVOKABLE。
- `ConnectionCatalog`：维护 `ConnectionProfile` 列表，读写 `<AppData>/OpenShell/connections/<id>.json`。
- `SessionController`：活动会话列表的 stub；真正的 SSH 流应该在 worker thread 上跑，通过信号回传 `sessionOutput`。
- `SettingsStore`：`QSettings` 封装，保存语言、窗口几何、最小化到托盘等。
- `TranslationManager`：运行时切换语言（system / en / zh_CN），从 `:/i18n/OpenShell_*.qm` 加载。
- `TrayController`：系统托盘菜单，包括显示/隐藏、连接快速打开、语言、退出。
- `MainWindow.qml`：主窗口 + SplitView + Sidebar + SessionTabs + TerminalView + FileBrowser。
- `Sidebar.qml`：左侧连接列表，支持过滤、右键编辑/删除。
- `ConnectionEditor.qml`：新建/编辑连接对话框。
- `TerminalView.qml`：终端 stub（一个只读 `TextArea` + 命令行）。
- `FileBrowser.qml`：本地/远程双栏 stub。
- `AccentCard.qml`：通用带左侧彩色细条的卡片。

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
    QString password;    // ⚠️ 明文，等待 QtKeychain 接入
    QString privateKeyPath;
    QString group;
    QString notes;
    int     lastUsedEpoch = 0;
};
```

存储位置：`QStandardPaths::AppDataLocation/connections/<id>.json`，每个连接一个文件，便于云同步 / Git 跟踪。

## 开发约定

- C++20。
- 新增 C++ 源文件后必须同步 `CMakeLists.txt`。
- 新增 QML 文件后必须加入 `qt_add_qml_module` 的 `QML_FILES`。
- 新增用户可见字符串 → 更新 `translations/OpenShell_zh_CN.ts`。
- 不要把密码、私钥等敏感内容写进日志。
- 修改 CI 前先读 `.github/workflows/release.yml`。

## 待办（按优先级）

1. **接入真实 SSH**：选 `libssh2` 或 `QSsh`，在 `SessionController` 起 worker thread，把 `sessionOutput` 信号串到 `TerminalView` 的 `TextArea.append()`。
2. **终端渲染**：从 stub 的 `TextArea` 升级为 vt100 解析（自研或 `QTermWidget`），处理颜色、光标、resize。
3. **SFTP 双栏**：libssh2 sftp 子模块，本地用 `QDir`/`QFileSystemModel`，远程对接 sftp。
4. **凭据加密**：`QtKeychain` 替换 JSON 明文密码字段，私钥保留路径不存内容。
5. **跳板机 / 端口转发 / 断线重连**：在 SessionController 的协议层上累加。
6. **状态监控**：通过 ssh exec `top -bn1` / `vmstat` 解析，画 dashboard。
7. **多语言**：补 `ja_JP`、`en` 翻译；现在 en 走 source fallback，zh_CN 完整。
8. **加密 / 同步**：保存的 connections 文件可选 GPG / age 加密，方便 Git 同步。
9. **打包**：参照 DeskPal 的 release.yml（已 copy），加 windeployqt 收紧旗标。
10. **自动化测试**：扩 `test_connection_catalog`，再加 `test_session_controller` 模拟 SSH stream。

## 常见坑

- Windows 上要装 Qt `msvc2022_64`，不要混用 MinGW。
- macOS 上的 Qt 目录是 `Qt/<version>/macos`。
- Linux 上 `xcb-cursor0` / `libxkbcommon-x11-0` 等 X11 依赖要装齐。
- `QtKeychain` 是第三方库（非 Qt 官方），引入时记得更新 CMake 和 CI 缓存键。
- 翻译 `.ts` 后缀和 TypeScript 冲突，IDE 可能误报；不影响 lrelease。

## 当前文件清单速查

```text
src/    AppController, ConnectionCatalog, SessionController, SettingsStore,
        TranslationManager, TrayController
qml/    MainWindow, Sidebar, ConnectionList(stub), ConnectionEditor,
        SessionTabs, TerminalView, FileBrowser, AccentCard
tests/  test_connection_catalog.cpp（QTest，OPENSHELL_BUILD_TESTS=ON 启用）
```
