# OpenShell

[English](README.md) | 简体中文

OpenShell 是一个基于 Qt 6 / QML / C++20 的跨平台 SSH / SFTP 终端工具，目标体验类似 FinalShell：左侧连接管理，中间多标签终端，下方本地/远程双栏文件管理。

当前项目已经具备可用骨架和部分真实能力：SSH 登录、终端渲染、SFTP 浏览与传输、远程文件打开/编辑回传、系统托盘和基础多语言。

## 功能

- 连接 Profile 管理：名称、协议、主机、端口、用户名、密码/私钥/Agent、分组和备注。
- SSH 终端会话：基于 libssh2 的真实连接，支持多标签、窗口 resize、Ctrl+C 中断。
- 终端渲染：自研 libvterm + `QQuickPaintedItem` 渲染，支持清屏、光标、基础颜色和文本属性。
- 终端选择复制：拖选复制、全选、双击选词、三击选行、Shift 扩展选择、选中即复制。
- SFTP 双栏文件管理：本地/远程目录浏览，排序，上传、下载、删除、重命名、新建文件/文件夹、权限修改。
- 远程目录同步：可将远程文件栏自动同步到终端 prompt 所在目录。
- 远程文件打开：双击远程文件可用系统默认应用、指定文本编辑器或内置编辑器打开。
- 编辑回传：外部编辑器保存后自动上传回远程；内置编辑器保存时上传回远程。
- 系统托盘：支持隐藏/显示窗口、快速打开连接、切换语言、退出。
- 多语言：默认支持简体中文，英文走源码 fallback。

## 环境要求

- Qt 6.6+，需要 `Quick`、`QuickControls2`、`Widgets`、`Network`、`Concurrent`、`LinguistTools`，测试需要 `Test`。
- CMake 3.16+。
- C++20 编译器：Windows 推荐 MSVC 2022，Linux/macOS 可用 GCC/Clang。
- Windows 推荐安装 Qt `msvc2022_64` 套件，不要混用 MinGW。

## 构建

### Windows 推荐脚本

```bat
run-vs-debug.bat
```

如需指定 CMake 或 Qt 路径：

```bat
set "CMAKE_EXE=E:\Qt\Tools\CMake_64\bin\cmake.exe"
set "QT_PREFIX=E:\Qt\6.11.1\msvc2022_64"
run-vs-debug.bat
```

部署 Qt 运行时：

```bat
deploy-vs-debug.bat
deploy-vs-release.bat
```

### CMake

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build --config Debug
```

启用测试：

```bash
cmake -S . -B build -DOPENSHELL_BUILD_TESTS=ON -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build --config Debug
ctest --test-dir build
```

## 项目结构

```text
.
├── CMakeLists.txt
├── Main.qml
├── src/
│   ├── AppController.*          # 应用中枢，连接 QML、会话、SFTP、设置、托盘
│   ├── ConnectionCatalog.*      # 连接 Profile 读写
│   ├── SessionController.*      # 活动 SSH 会话管理
│   ├── SettingsStore.*          # QSettings 封装
│   ├── TranslationManager.*     # 语言切换
│   ├── TrayController.*         # 系统托盘
│   ├── ssh/                     # libssh2 SSH/SFTP worker 与目录操作
│   └── terminal/                # libvterm 屏幕模型与 QML 绘制控件
├── qml/
│   ├── MainWindow.qml
│   ├── Sidebar.qml
│   ├── ConnectionEditor.qml
│   ├── SessionTabs.qml
│   ├── TerminalView.qml
│   ├── FileBrowser.qml
│   └── AccentCard.qml
├── translations/
│   └── OpenShell_zh_CN.ts
├── tests/
│   ├── test_connection_catalog.cpp
│   └── test_session_controller.cpp
└── docs/
    └── architecture.md
```

## 数据存储

连接配置保存到：

```text
QStandardPaths::AppDataLocation/connections/<id>.json
```

目前密码和私钥口令仍可能以明文形式保存在本地配置中，后续应接入 QtKeychain 或系统 Credential Store。

远程文件“打开/编辑”会先下载到系统临时目录：

```text
<Temp>/OpenShell/remote-open/<uuid>/
```

启用自动回传时，OpenShell 会监听该临时文件修改并上传回原远程路径。

## 常用操作

- 双击左侧连接：打开 SSH 会话。
- 终端右键：复制、粘贴、全选、发送 Ctrl+C、清屏。
- 终端拖选：松开后自动复制。
- 终端双击/三击：选词/选行并自动复制。
- 远程文件栏双击目录：进入目录。
- 远程文件栏双击文件：按设置打开文件。
- 远程栏同步按钮：开启后跟随终端当前目录。
- 远程栏设置按钮：选择系统默认应用、指定文本编辑器或内置编辑器。

## 支持项目

如果 OpenShell 对你有帮助，欢迎请我喝杯咖啡。国内用户可以使用微信支付或支付宝扫码支持：

<p align="center">
  <img src="assets/support/wechat-pay.jpg" alt="微信支付赞赏码" width="260">
  <img src="assets/support/alipay.png" alt="支付宝收款码" width="260">
</p>

国际用户也可以通过 PayPal 支持：[paypal.me/wangchaozhi123](https://paypal.me/wangchaozhi123)

## 后续计划

- 凭据加密：接入 QtKeychain 或系统 Credential Store。
- 更完整的终端能力：滚屏历史、搜索、更多 VT 序列兼容、复制格式细节。
- SFTP 增强：传输队列、断点续传、冲突处理、批量操作。
- SSH 增强：跳板机、端口转发、断线重连、Agent 更完整支持。
- 服务器监控：通过 SSH exec 采集 CPU、内存、磁盘、网络状态并展示 dashboard。
- 打包发布：完善 Windows/macOS/Linux release 流程。

更多设计说明见 [docs/architecture.md](docs/architecture.md) 和 [AGENTS.md](AGENTS.md)。
