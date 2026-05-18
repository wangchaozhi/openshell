# OpenShell

[English](README.md) | [简体中文](README-CN.md) | 日本語

OpenShell は Qt 6 / QML / C++20 で構築されたクロスプラットフォームの SSH / SFTP ターミナルクライアントです。FinalShell に近い操作感を目指しており、左側に接続管理、中央にマルチタブターミナル、下部にローカル/リモートのデュアルペインファイルブラウザーを配置しています。

本プロジェクトはすでに実用的な機能を備えています：SSH 接続、ターミナルレンダリング、SFTP ブラウジングと転送、リモートファイルの開く/編集/アップロード、システムトレイ、基本的な多言語対応。

![OpenShell スクリーンショット](assets/screenshots/Snipaste_2026-05-18_16-21-53.png)

## 機能

- 接続プロファイル管理：名前、プロトコル、ホスト、ポート、ユーザー名、パスワード/秘密鍵/エージェント認証、グループ、メモ。
- SSH ターミナルセッション：libssh2 ベースの実接続、マルチタブ、リサイズ対応、Ctrl+C 割り込みサポート。
- ターミナルレンダリング：libvterm スクリーンモデルをカスタム `QQuickPaintedItem` でレンダリング、カーソル・クリア・基本色・テキスト属性に対応。
- ターミナル選択コピー：ドラッグ選択、全選択、ダブルクリック単語選択、トリプルクリック行選択、Shift 拡張選択、選択時自動コピー。
- SFTP デュアルペインファイルブラウザー：ローカル/リモートのディレクトリ閲覧、ソート、アップロード、ダウンロード、削除、名前変更、ファイル/フォルダー作成、chmod。
- リモートディレクトリ同期：ターミナルのプロンプトが示すカレントディレクトリとリモートファイルブラウザーを自動同期。
- リモートファイルオープン：リモートファイルをダブルクリックして、システム既定アプリ・指定エディター・内蔵エディターで開く。
- 編集アップロードバック：外部エディターで保存後に自動でリモートへアップロード。内蔵エディターは保存時にアップロード。
- システムトレイ：ウィンドウの表示/非表示、接続のクイックオープン、言語切り替え、終了。
- 多言語対応：簡体字中国語・日本語を内蔵。英語はソース文字列フォールバック。

## 動作要件

- Qt 6.6 以上（`Quick`、`QuickControls2`、`Widgets`、`Network`、`Concurrent`、`LinguistTools` モジュールが必要。テストには `Test` も必要）。
- CMake 3.16 以上。
- C++20 対応コンパイラー：Windows では MSVC 2022 を推奨。Linux/macOS では GCC/Clang も利用可能。
- Windows では Qt の `msvc2022_64` キットを使用してください。MinGW との混在は避けてください。

## ビルド

### Windows 推奨スクリプト

```bat
run-vs-debug.bat
```

CMake または Qt のパスを指定する場合：

```bat
set "CMAKE_EXE=E:\Qt\Tools\CMake_64\bin\cmake.exe"
set "QT_PREFIX=E:\Qt\6.11.1\msvc2022_64"
run-vs-debug.bat
```

Qt ランタイムのデプロイ：

```bat
deploy-vs-debug.bat
deploy-vs-release.bat
```

### CMake

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build --config Debug
```

テストを有効にする場合：

```bash
cmake -S . -B build -DOPENSHELL_BUILD_TESTS=ON -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build --config Debug
ctest --test-dir build
```

## プロジェクト構成

```text
.
├── CMakeLists.txt
├── Main.qml
├── src/
│   ├── AppController.*          # アプリケーションファサード（QML・セッション・SFTP・設定・トレイ）
│   ├── ConnectionCatalog.*      # 接続プロファイルの永続化
│   ├── SessionController.*      # アクティブSSHセッション管理
│   ├── SettingsStore.*          # QSettings ラッパー
│   ├── TranslationManager.*     # 実行時言語切り替え
│   ├── TrayController.*         # システムトレイ統合
│   ├── ssh/                     # libssh2 SSH/SFTP ワーカーとリモートファイル操作
│   └── terminal/                # libvterm スクリーンモデルと QML ターミナルウィジェット
├── qml/
│   ├── MainWindow.qml
│   ├── Sidebar.qml
│   ├── ConnectionEditor.qml
│   ├── SessionTabs.qml
│   ├── TerminalView.qml
│   ├── FileBrowser.qml
│   └── AccentCard.qml
├── translations/
│   ├── OpenShell_zh_CN.ts
│   └── OpenShell_ja_JP.ts
├── tests/
│   ├── test_connection_catalog.cpp
│   └── test_session_controller.cpp
└── docs/
    └── architecture.md
```

## データストレージ

接続プロファイルの保存先：

```text
QStandardPaths::AppDataLocation/connections/<id>.json
```

パスワードや秘密鍵のパスフレーズは現時点でローカルの設定ファイルに平文で保存される場合があります。将来のバージョンでは QtKeychain または OS のクレデンシャルストアへの統合を予定しています。

リモートファイルのオープン/編集ワークフローでは、まずファイルをシステムの一時ディレクトリにダウンロードします：

```text
<Temp>/OpenShell/remote-open/<uuid>/
```

アップロードバックが有効な場合、OpenShell は一時ファイルの変更を監視し、元のリモートパスに自動でアップロードします。

## 主な操作

- 接続をダブルクリック：SSH セッションを開く。
- ターミナルを右クリック：コピー、貼り付け、全選択、Ctrl+C 送信、画面クリア。
- ターミナルでドラッグ選択：マウスリリース時に自動コピー。
- ターミナルでダブルクリック/トリプルクリック：単語/行を選択して自動コピー。
- リモートのディレクトリをダブルクリック：そのディレクトリに移動。
- リモートのファイルをダブルクリック：設定に従ってファイルを開く。
- リモート同期ボタン：ターミナルのカレントディレクトリに追従。
- リモート設定ボタン：システム既定アプリ・指定テキストエディター・内蔵エディターを選択。

## サポート

OpenShell は余暇に開発されています。役に立った場合は、コーヒー一杯のサポートをいただけると嬉しいです。

[PayPal でサポート](https://paypal.me/wangchaozhi123)

## ロードマップ

- クレデンシャルセキュリティ：QtKeychain または OS クレデンシャルストアへの統合。
- ターミナル強化：スクロールバック履歴、検索、より広い VT 互換性、コピー書式の改善。
- SFTP 強化：転送キュー、レジューム対応、競合処理、バッチ操作。
- SSH 強化：踏み台ホスト、ポートフォワーディング、再接続、エージェントの完全サポート。
- サーバー監視：SSH exec 経由で CPU・メモリ・ディスク・ネットワーク統計を収集してダッシュボードに表示。
- パッケージング：Windows/macOS/Linux のリリースワークフローの整備。

詳細なアーキテクチャについては [docs/architecture.md](docs/architecture.md) および [AGENTS.md](AGENTS.md) を参照してください。
