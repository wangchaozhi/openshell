#!/usr/bin/env bash
# OpenShell macOS Debug 一键跑：杀掉已有进程 -> 配置 -> 编译 -> 启动 .app。
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-mac}"
APP_BUNDLE="$BUILD_DIR/bin/OpenShell.app"
APP_BIN="$APP_BUNDLE/Contents/MacOS/OpenShell"

stop_running_app() {
    local target="$1"
    if [[ -x "$target" ]]; then
        pkill -f "$target" >/dev/null 2>&1 || true
    fi
}

stop_running_app "$APP_BIN"

export BUILD_TYPE="Debug"
export BUILD_DIR
"$ROOT_DIR/build-mac.sh"

echo "Launching $APP_BUNDLE ..."
open -n "$APP_BUNDLE"
