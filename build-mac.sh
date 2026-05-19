#!/usr/bin/env bash
# OpenShell macOS build: 若 build-mac 未配置则先调用 configure-mac.sh，然后 ninja 编译。
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-mac}"

if [[ ! -f "$BUILD_DIR/build.ninja" ]]; then
    BUILD_DIR="$BUILD_DIR" "$ROOT_DIR/configure-mac.sh"
fi

if [[ -z "${CMAKE_EXE:-}" ]]; then
    if [[ -x "$HOME/Qt/Tools/CMake/CMake.app/Contents/bin/cmake" ]]; then
        CMAKE_EXE="$HOME/Qt/Tools/CMake/CMake.app/Contents/bin/cmake"
    elif command -v cmake >/dev/null 2>&1; then
        CMAKE_EXE="$(command -v cmake)"
    else
        echo "CMake was not found." >&2
        exit 1
    fi
fi

"$CMAKE_EXE" --build "$BUILD_DIR"

APP_BUNDLE="$BUILD_DIR/bin/OpenShell.app"
if [[ -d "$APP_BUNDLE" ]]; then
    echo "macOS app bundle: $APP_BUNDLE"
else
    echo "OpenShell.app was not found after build." >&2
    exit 1
fi
