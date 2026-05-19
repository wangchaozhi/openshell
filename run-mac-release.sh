#!/usr/bin/env bash
# OpenShell macOS Release 一键跑：使用单独的 build-mac-release 目录避免与 Debug 串台。
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-mac-release}"
APP_BUNDLE="$BUILD_DIR/bin/OpenShell.app"
APP_BIN="$APP_BUNDLE/Contents/MacOS/OpenShell"

stop_running_app() {
    local target="$1"
    if [[ -x "$target" ]]; then
        pkill -f "$target" >/dev/null 2>&1 || true
    fi
}

stop_running_app "$APP_BIN"

export BUILD_TYPE="Release"
export BUILD_DIR
"$ROOT_DIR/build-mac.sh"

echo "Launching $APP_BUNDLE ..."
open -n "$APP_BUNDLE"
