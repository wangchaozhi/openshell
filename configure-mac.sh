#!/usr/bin/env bash
# OpenShell macOS configure: 用 Ninja 生成 build-mac 目录。
# 关键变量可通过环境变量覆盖：QT_PREFIX / CMAKE_EXE / NINJA_EXE / BUILD_TYPE。
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-mac}"
BUILD_TYPE="${BUILD_TYPE:-Debug}"

detect_qt_prefix() {
    if [[ -n "${QT_PREFIX:-}" && -f "$QT_PREFIX/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
        return
    fi
    local candidates=()
    if [[ -d "$HOME/Qt" ]]; then
        while IFS= read -r -d '' p; do candidates+=("$p"); done < <(find "$HOME/Qt" -maxdepth 2 -type d -name macos -print0 2>/dev/null)
    fi
    for p in /opt/Qt/*/macos /Applications/Qt/*/macos; do
        [[ -d "$p" ]] && candidates+=("$p")
    done
    # 取版本号最大的一个
    local best=""
    for p in "${candidates[@]}"; do
        if [[ -f "$p/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
            if [[ -z "$best" || "$p" > "$best" ]]; then
                best="$p"
            fi
        fi
    done
    if [[ -n "$best" ]]; then
        QT_PREFIX="$best"
    fi
}

detect_cmake() {
    if [[ -n "${CMAKE_EXE:-}" && -x "$CMAKE_EXE" ]]; then
        return
    fi
    if [[ -x "$HOME/Qt/Tools/CMake/CMake.app/Contents/bin/cmake" ]]; then
        CMAKE_EXE="$HOME/Qt/Tools/CMake/CMake.app/Contents/bin/cmake"
        return
    fi
    if command -v cmake >/dev/null 2>&1; then
        CMAKE_EXE="$(command -v cmake)"
    fi
}

detect_ninja() {
    if [[ -n "${NINJA_EXE:-}" && -x "$NINJA_EXE" ]]; then
        return
    fi
    if [[ -x "$HOME/Qt/Tools/Ninja/ninja" ]]; then
        NINJA_EXE="$HOME/Qt/Tools/Ninja/ninja"
        return
    fi
    if command -v ninja >/dev/null 2>&1; then
        NINJA_EXE="$(command -v ninja)"
    fi
}

detect_qt_prefix
detect_cmake
detect_ninja

if [[ -z "${QT_PREFIX:-}" ]]; then
    echo "Qt for macOS was not found." >&2
    echo "  Set QT_PREFIX, e.g.: export QT_PREFIX=\"\$HOME/Qt/6.11.1/macos\"" >&2
    exit 1
fi
if [[ -z "${CMAKE_EXE:-}" ]]; then
    echo "CMake was not found. Install it or set CMAKE_EXE." >&2
    exit 1
fi
if [[ -z "${NINJA_EXE:-}" ]]; then
    echo "Ninja was not found. Install it or set NINJA_EXE." >&2
    exit 1
fi

echo "Configuring macOS build..."
echo "  QT_PREFIX  = $QT_PREFIX"
echo "  CMAKE_EXE  = $CMAKE_EXE"
echo "  NINJA_EXE  = $NINJA_EXE"
echo "  BUILD_DIR  = $BUILD_DIR"
echo "  BUILD_TYPE = $BUILD_TYPE"

"$CMAKE_EXE" \
    -S "$ROOT_DIR" \
    -B "$BUILD_DIR" \
    -G Ninja \
    -DCMAKE_PREFIX_PATH="$QT_PREFIX" \
    -DCMAKE_MAKE_PROGRAM="$NINJA_EXE" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

echo "macOS configure finished: $BUILD_DIR"
