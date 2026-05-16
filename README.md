# OpenShell

English | [简体中文](README-CN.md)

OpenShell is a cross-platform SSH / SFTP terminal client built with Qt 6, QML, and C++20. It aims for a workflow similar to FinalShell: connection management on the left, multi-tab terminals in the main area, and a local/remote file browser below.

The project is no longer only a UI skeleton. It already includes real SSH sessions, terminal rendering, SFTP browsing and transfer operations, remote file open/edit/upload-back workflows, a system tray, and basic localization.

## Features

- Connection profiles: name, protocol, host, port, username, password/private key/agent auth, group, and notes.
- SSH terminal sessions: real libssh2-backed connections, multi-tab sessions, resize handling, and Ctrl+C interrupt support.
- Terminal rendering: libvterm screen model rendered through a custom `QQuickPaintedItem`, with cursor, clear screen, basic colors, and text attributes.
- Terminal selection and copy: drag selection, select all, double-click word selection, triple-click line selection, Shift-extend selection, and copy-on-selection.
- SFTP dual-pane file browser: local/remote directory browsing, sorting, upload, download, delete, rename, create file/folder, and chmod.
- Remote directory sync: optionally sync the remote file browser to the current directory detected from the terminal prompt.
- Remote file opening: double-click remote files and open them with the system default app, a configured editor, or the built-in editor.
- Edit upload-back: external editor saves can be watched and uploaded back to the original remote path; the built-in editor uploads on save.
- System tray: show/hide window, quick-open connections, language switching, and quit.
- Localization: Simplified Chinese is included; English currently uses source-string fallback.

## Requirements

- Qt 6.6+ with `Quick`, `QuickControls2`, `Widgets`, `Network`, `Concurrent`, and `LinguistTools`; tests also require `Test`.
- CMake 3.16+.
- A C++20 compiler: MSVC 2022 is recommended on Windows; GCC/Clang work on Linux/macOS.
- On Windows, use the Qt `msvc2022_64` kit. Do not mix it with MinGW.

## Build

### Recommended Windows Scripts

```bat
run-vs-debug.bat
```

Override CMake or Qt paths when needed:

```bat
set "CMAKE_EXE=E:\Qt\Tools\CMake_64\bin\cmake.exe"
set "QT_PREFIX=E:\Qt\6.11.1\msvc2022_64"
run-vs-debug.bat
```

Deploy Qt runtime files:

```bat
deploy-vs-debug.bat
deploy-vs-release.bat
```

### CMake

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build --config Debug
```

Enable tests:

```bash
cmake -S . -B build -DOPENSHELL_BUILD_TESTS=ON -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build --config Debug
ctest --test-dir build
```

## Project Layout

```text
.
├── CMakeLists.txt
├── Main.qml
├── src/
│   ├── AppController.*          # Application facade for QML, sessions, SFTP, settings, and tray
│   ├── ConnectionCatalog.*      # Connection profile persistence
│   ├── SessionController.*      # Active SSH session management
│   ├── SettingsStore.*          # QSettings wrapper
│   ├── TranslationManager.*     # Runtime language switching
│   ├── TrayController.*         # System tray integration
│   ├── ssh/                     # libssh2 SSH/SFTP workers and remote file operations
│   └── terminal/                # libvterm screen model and QML-painted terminal widget
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

## Data Storage

Connection profiles are stored at:

```text
QStandardPaths::AppDataLocation/connections/<id>.json
```

Passwords and private key passphrases may still be stored in local plaintext configuration. A future version should integrate QtKeychain or the host OS credential store.

Remote file open/edit workflows first download files to:

```text
<Temp>/OpenShell/remote-open/<uuid>/
```

When upload-back is enabled, OpenShell watches the temporary file and uploads changes back to the original remote path.

## Common Operations

- Double-click a connection in the sidebar to open an SSH session.
- Right-click the terminal for Copy, Paste, Select All, Send Ctrl+C, and Clear Screen.
- Drag-select terminal text to copy on mouse release.
- Double-click or triple-click terminal text to select a word or line and copy immediately.
- Double-click a remote directory to enter it.
- Double-click a remote file to open it according to the configured open mode.
- Use the remote sync button to follow the terminal's current directory.
- Use the remote settings button to choose the system default app, a configured text editor, or the built-in editor.

## Support

OpenShell is built in spare time. If it saves you time, a coffee would be appreciated.

[Support via PayPal](https://paypal.me/wangchaozhi123)

## Roadmap

- Credential security: integrate QtKeychain or the host OS credential store.
- Terminal improvements: scrollback history, search, broader VT compatibility, and copy formatting refinements.
- SFTP improvements: transfer queue, resume support, conflict handling, and batch operations.
- SSH improvements: jump hosts, port forwarding, reconnects, and fuller agent support.
- Server monitoring: collect CPU, memory, disk, and network stats through SSH exec and render a dashboard.
- Packaging: improve Windows/macOS/Linux release workflows.

See [docs/architecture.md](docs/architecture.md) and [AGENTS.md](AGENTS.md) for more project notes.
