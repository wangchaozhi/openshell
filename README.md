# OpenShell

跨平台 SSH / SFTP 终端工具骨架（Qt 6 / QML / C++20，类 FinalShell）。

## 目标

- 多 Tab 终端、SFTP 双栏文件浏览、服务器状态监控。
- 保存的连接 Profile：SSH 密码 / 私钥 / Agent，分组与备注。
- 系统托盘常驻、关窗隐藏到托盘。
- 多语言：简体中文 + 英文（系统语言自动落点）。
- 跨平台：Windows / macOS / Linux。

> ⚠️ 当前仓库只是**骨架**：终端输出、SSH 连接、SFTP 列表都是 stub。要让它真正能登录服务器，需要在 `SessionController` 接入 `libssh2` / `Qt6SerialBus` / 自研协议。

## 环境

- Qt 6.6+ （`Quick`、`QuickControls2`、`Widgets`、`Network`、`Concurrent`、`LinguistTools`，测试时另需 `Test`）
- CMake 3.16+
- 支持 C++20 的编译器（MSVC 2022 / Clang 14+ / GCC 11+）

## 构建

### Visual Studio 脚本（Windows，推荐）

```bat
run-vs-debug.bat
```

需要 CMake / Qt MSVC 套件时可临时覆盖：

```bat
set "CMAKE_EXE=E:\Qt\Tools\CMake_64\bin\cmake.exe"
set "QT_PREFIX=E:\Qt\6.11.1\msvc2022_64"
run-vs-debug.bat
```

部署 Qt 运行时到 `build-vs\bin\Debug` 或 `\Release`：

```bat
deploy-vs-debug.bat
deploy-vs-release.bat
```

### CMake 直接调

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build --config Release
```

## 项目结构

```text
.
├─ main.cpp
├─ Main.qml
├─ CMakeLists.txt
├─ src/
│  ├─ AppController.*
│  ├─ ConnectionCatalog.*    # 保存连接 Profile 到 AppData JSON
│  ├─ SessionController.*    # 活动会话 stub
│  ├─ SettingsStore.*        # QSettings 封装
│  ├─ TranslationManager.*   # zh_CN / en 切换
│  └─ TrayController.*       # 系统托盘菜单
├─ qml/
│  ├─ MainWindow.qml
│  ├─ Sidebar.qml            # 左侧连接列表
│  ├─ ConnectionEditor.qml   # 新建/编辑连接对话框
│  ├─ SessionTabs.qml
│  ├─ TerminalView.qml       # 终端 stub
│  ├─ FileBrowser.qml        # 本地/远程 stub
│  └─ AccentCard.qml
├─ translations/
│  └─ OpenShell_zh_CN.ts
├─ assets/icons/openshell.svg
├─ tests/test_connection_catalog.cpp  # QTest（需 -DOPENSHELL_BUILD_TESTS=ON）
├─ docs/architecture.md
└─ .github/workflows/release.yml
```

## 单元测试

```bash
cmake -S . -B build -DOPENSHELL_BUILD_TESTS=ON -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build --target test_connection_catalog
ctest --test-dir build
```

## 后续

- 接入 SSH 协议栈（libssh2 / QtSSH / 自研），让 `SessionController::open` 真正建立 transport。
- 终端渲染：`QTermWidget` 或自研 vt100 解析 + Canvas/QQuickPaintedItem。
- SFTP：libssh2 的 sftp 子模块 + 双栏 ListView。
- 密码加密：当前明文写在 JSON，需要在 `SettingsStore` 加 `QtKeychain` 或 OS Credential Store。
- 端口转发、跳板机链、断线重连、stats（CPU/Mem 通过 ssh exec `top` / `vmstat`）。

更多见 [docs/architecture.md](docs/architecture.md) 与 [AGENTS.md](AGENTS.md)。
