#!/usr/bin/env bash
# OpenShell Android build (macOS). 默认编 arm64-v8a，Apple Silicon 模拟器原生
# 可跑该 ABI。可覆盖：JAVA_HOME / ANDROID_SDK_ROOT / ANDROID_NDK_ROOT /
# QT_ANDROID_PREFIX / CMAKE_EXE / NINJA_EXE / BUILD_DIR / ANDROID_ABI /
# ANDROID_PLATFORM。
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ANDROID_ABI="${ANDROID_ABI:-arm64-v8a}"
ANDROID_PLATFORM="${ANDROID_PLATFORM:-android-35}"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-android-${ANDROID_ABI}}"
APP_NAME="OpenShell"
APK_PATH="${BUILD_DIR}/android-build/${APP_NAME}.apk"

# JDK 17 或 21。AGP 的 jdkImageTransform 在 GraalVM 的 jlink 上会因为缺
# `jdk.internal.vm.ci` 模块跑挂，所以优先跳过 GraalVM/Oracle GraalVM，
# 找普通 OpenJDK（Homebrew / Temurin / Liberica / Zulu 等）。
java_is_graalvm() {
    local home="$1"
    [[ -x "$home/bin/java" ]] || return 0
    "$home/bin/java" -version 2>&1 | grep -qiE "graalvm" && return 0
    return 1
}

detect_java_home() {
    if [[ -n "${JAVA_HOME:-}" && -x "$JAVA_HOME/bin/java" ]]; then
        return
    fi
    # Homebrew 装的 openjdk@21 默认不软链到 /Library/JavaVirtualMachines，
    # 直接看 brew 的前缀比 java_home 更可靠。
    for prefix in "$(brew --prefix openjdk@21 2>/dev/null)" \
                  "$(brew --prefix openjdk@17 2>/dev/null)"; do
        if [[ -n "$prefix" && -x "$prefix/libexec/openjdk.jdk/Contents/Home/bin/java" ]]; then
            JAVA_HOME="$prefix/libexec/openjdk.jdk/Contents/Home"
            return
        fi
    done
    for v in 21 17; do
        local lines
        lines="$(/usr/libexec/java_home -v "$v" -V 2>&1 | awk -F'"' '/^[ ]+[0-9]/ {print $NF}')"
        while IFS= read -r home; do
            [[ -z "$home" ]] && continue
            if [[ -x "$home/bin/java" ]] && ! java_is_graalvm "$home"; then
                JAVA_HOME="$home"
                return
            fi
        done <<< "$lines"
    done
    # 实在没有非 GraalVM 的就退而求其次（让 brew 装的更优先）。
    for v in 21 17; do
        local home
        home="$(/usr/libexec/java_home -v "$v" 2>/dev/null || true)"
        if [[ -n "$home" && -x "$home/bin/java" ]]; then
            JAVA_HOME="$home"
            return
        fi
    done
}

detect_android_sdk() {
    if [[ -n "${ANDROID_SDK_ROOT:-}" && -d "$ANDROID_SDK_ROOT/platform-tools" ]]; then
        return
    fi
    for p in "$HOME/Library/Android/sdk" "$HOME/Android/Sdk" "${ANDROID_HOME:-}"; do
        if [[ -n "$p" && -d "$p/platform-tools" ]]; then
            ANDROID_SDK_ROOT="$p"
            return
        fi
    done
}

detect_android_ndk() {
    if [[ -n "${ANDROID_NDK_ROOT:-}" && -f "$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" ]]; then
        return
    fi
    if [[ -d "$ANDROID_SDK_ROOT/ndk" ]]; then
        local best=""
        while IFS= read -r -d '' d; do
            if [[ -f "$d/build/cmake/android.toolchain.cmake" ]]; then
                if [[ -z "$best" || "$d" > "$best" ]]; then
                    best="$d"
                fi
            fi
        done < <(find "$ANDROID_SDK_ROOT/ndk" -mindepth 1 -maxdepth 1 -type d -print0)
        if [[ -n "$best" ]]; then
            ANDROID_NDK_ROOT="$best"
        fi
    fi
}

detect_qt_android() {
    if [[ -n "${QT_ANDROID_PREFIX:-}" && -x "$QT_ANDROID_PREFIX/bin/qt-cmake" ]]; then
        return
    fi
    case "$ANDROID_ABI" in
        arm64-v8a)   kit="android_arm64_v8a" ;;
        armeabi-v7a) kit="android_armv7" ;;
        x86_64)      kit="android_x86_64" ;;
        x86)         kit="android_x86" ;;
        *) echo "Unsupported ABI: $ANDROID_ABI" >&2; exit 1 ;;
    esac
    if [[ -d "$HOME/Qt" ]]; then
        local best=""
        while IFS= read -r -d '' p; do
            if [[ -x "$p/bin/qt-cmake" ]]; then
                if [[ -z "$best" || "$p" > "$best" ]]; then
                    best="$p"
                fi
            fi
        done < <(find "$HOME/Qt" -maxdepth 2 -type d -name "$kit" -print0 2>/dev/null)
        if [[ -n "$best" ]]; then
            QT_ANDROID_PREFIX="$best"
        fi
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

detect_java_home
detect_android_sdk
detect_android_ndk
detect_qt_android
detect_cmake
detect_ninja

[[ -z "${JAVA_HOME:-}" ]] && { echo "JDK 17 or 21 not found. Set JAVA_HOME." >&2; exit 1; }
[[ -z "${ANDROID_SDK_ROOT:-}" ]] && { echo "Android SDK not found. Set ANDROID_SDK_ROOT." >&2; exit 1; }
[[ -z "${ANDROID_NDK_ROOT:-}" ]] && { echo "Android NDK not found. Set ANDROID_NDK_ROOT." >&2; exit 1; }
[[ -z "${QT_ANDROID_PREFIX:-}" ]] && { echo "Qt Android $ANDROID_ABI kit not found. Set QT_ANDROID_PREFIX." >&2; exit 1; }
[[ -z "${CMAKE_EXE:-}" ]] && { echo "CMake not found. Set CMAKE_EXE." >&2; exit 1; }
[[ -z "${NINJA_EXE:-}" ]] && { echo "Ninja not found. Set NINJA_EXE." >&2; exit 1; }

export JAVA_HOME
export ANDROID_SDK_ROOT
export ANDROID_NDK_ROOT
export PATH="$JAVA_HOME/bin:$(dirname "$NINJA_EXE"):$PATH"

echo "Configuring Android $ANDROID_ABI build..."
echo "  JAVA_HOME         = $JAVA_HOME"
echo "  ANDROID_SDK_ROOT  = $ANDROID_SDK_ROOT"
echo "  ANDROID_NDK_ROOT  = $ANDROID_NDK_ROOT"
echo "  QT_ANDROID_PREFIX = $QT_ANDROID_PREFIX"
echo "  BUILD_DIR         = $BUILD_DIR"
echo "  ANDROID_ABI       = $ANDROID_ABI"
echo "  ANDROID_PLATFORM  = $ANDROID_PLATFORM"

"$QT_ANDROID_PREFIX/bin/qt-cmake" \
    -S "$ROOT_DIR" \
    -B "$BUILD_DIR" \
    -G Ninja \
    -DCMAKE_MAKE_PROGRAM="$NINJA_EXE" \
    -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" \
    -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT" \
    -DANDROID_ABI="$ANDROID_ABI" \
    -DANDROID_PLATFORM="$ANDROID_PLATFORM"

echo "Building $APP_NAME..."
"$CMAKE_EXE" --build "$BUILD_DIR"

if [[ -f "$APK_PATH" ]]; then
    echo "Android APK: $APK_PATH"
else
    echo "APK not found at $APK_PATH" >&2
    exit 1
fi
