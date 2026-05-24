#!/usr/bin/env bash
# OpenShell iOS Simulator build.
# 走单配置 Ninja —— Xcode / Ninja Multi-Config 都会把 mbedcrypto 静态库放进
# 配置子目录 (Debug-iphonesimulator/ 或 Debug/)，而 CMakeLists.txt 里
# MBEDCRYPTO_LIBRARY 写死的是无子目录路径，会触发链接错误。单配置 Ninja
# 把产物直接放在 library/ 下，绕开这条硬编码。需要切 Release 时用不同的
# BUILD_DIR / BUILD_TYPE 即可（默认 build-ios / Debug）。
# 可覆盖：QT_IOS_ROOT / QT_HOST_ROOT / BUILD_TYPE / BUILD_DIR / NINJA_EXE
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-ios}"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
APP_NAME="OpenShell"
# Qt 6.11.x 的 iOS 预编译 kit 用的是经典 fat（x86_64 simulator + arm64 device），
# 没有单独的 arm64-iphonesimulator slice。所以默认 x86_64 跑模拟器（Apple
# Silicon 上靠 Rosetta），真机 arm64 用 IOS_ARCH=arm64 + IOS_SYSROOT=iphoneos。
IOS_ARCH="${IOS_ARCH:-x86_64}"
IOS_SYSROOT="${IOS_SYSROOT:-iphonesimulator}"

detect_qt_root() {
    local kind="$1"   # ios | macos
    local var_name="$2"
    if [[ -n "${!var_name:-}" && -x "${!var_name}/bin/qt-cmake" ]]; then
        return
    fi
    if [[ -d "$HOME/Qt" ]]; then
        local best=""
        while IFS= read -r -d '' p; do
            if [[ -x "$p/bin/qt-cmake" ]]; then
                if [[ -z "$best" || "$p" > "$best" ]]; then
                    best="$p"
                fi
            fi
        done < <(find "$HOME/Qt" -maxdepth 2 -type d -name "$kind" -print0 2>/dev/null)
        if [[ -n "$best" ]]; then
            printf -v "$var_name" "%s" "$best"
        fi
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

detect_qt_root ios   QT_IOS_ROOT
detect_qt_root macos QT_HOST_ROOT
detect_ninja
detect_cmake

if [[ -z "${QT_IOS_ROOT:-}" ]]; then
    echo "Qt iOS kit not found. Set QT_IOS_ROOT, e.g.:" >&2
    echo "  export QT_IOS_ROOT=\"\$HOME/Qt/6.11.1/ios\"" >&2
    exit 1
fi
if [[ -z "${QT_HOST_ROOT:-}" ]]; then
    echo "Qt host (macOS) kit not found. Set QT_HOST_ROOT, e.g.:" >&2
    echo "  export QT_HOST_ROOT=\"\$HOME/Qt/6.11.1/macos\"" >&2
    exit 1
fi
if [[ -z "${NINJA_EXE:-}" ]]; then
    echo "Ninja not found. Install Ninja or set NINJA_EXE." >&2
    exit 1
fi
if [[ -z "${CMAKE_EXE:-}" ]]; then
    echo "CMake not found. Install it or set CMAKE_EXE." >&2
    exit 1
fi
if ! command -v xcrun >/dev/null 2>&1; then
    echo "xcrun not found. Install Xcode command-line tools." >&2
    exit 1
fi

echo "Configuring iOS build (single-config Ninja)..."
echo "  QT_IOS_ROOT   = $QT_IOS_ROOT"
echo "  QT_HOST_ROOT  = $QT_HOST_ROOT"
echo "  NINJA_EXE     = $NINJA_EXE"
echo "  BUILD_DIR     = $BUILD_DIR"
echo "  BUILD_TYPE    = $BUILD_TYPE"
echo "  SYSROOT       = $IOS_SYSROOT"
echo "  ARCH          = $IOS_ARCH"

"$QT_IOS_ROOT/bin/qt-cmake" \
    -S "$ROOT_DIR" \
    -B "$BUILD_DIR" \
    -G Ninja \
    -DCMAKE_MAKE_PROGRAM="$NINJA_EXE" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DQT_HOST_PATH="$QT_HOST_ROOT" \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_SYSROOT="$IOS_SYSROOT" \
    -DCMAKE_OSX_ARCHITECTURES="$IOS_ARCH"

echo "Building $APP_NAME ($BUILD_TYPE)..."
# 注意：qt-cmake 在某些 Qt 版本（如 6.10）只接配置期参数，不支持 --build。
# 直接用 cmake --build 走 ninja，跨版本更稳。
"$CMAKE_EXE" --build "$BUILD_DIR"

app_path="$(find "$BUILD_DIR" -type d -name "${APP_NAME}.app" -print -quit)"
if [[ -z "$app_path" ]]; then
    echo "iOS app bundle was not found." >&2
    exit 1
fi
echo "iOS .app: $app_path"

# MAKE_IPA=1 ./build-ios.sh 时打个 ad-hoc 签名的 .ipa。device 目标（iphoneos）
# 出来的 .ipa 能用在越狱 / TrollStore / AltStore 重签场景；simulator 目标也能
# 打但意义不大，simulator 只装 .app。
if [[ "${MAKE_IPA:-0}" = "1" ]]; then
    asset_name="${IPA_NAME:-OpenShell-ios-${IOS_SYSROOT}-${IOS_ARCH}}"
    ipa_path="$BUILD_DIR/${asset_name}.ipa"
    echo "Ad-hoc signing $app_path..."
    codesign --force --deep --sign - "$app_path"
    staging="$(mktemp -d)"
    mkdir -p "$staging/Payload"
    cp -R "$app_path" "$staging/Payload/"
    rm -f "$ipa_path"
    (cd "$staging" && zip -qry "$ipa_path" Payload)
    rm -rf "$staging"
    echo "iOS .ipa:  $ipa_path"
fi
