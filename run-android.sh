#!/usr/bin/env bash
# 编译并把 APK 装到当前连接的 Android 设备或模拟器。
# AVD_NAME 指定时若没有运行中的设备，会自动 boot 一个模拟器再装。
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ANDROID_ABI="${ANDROID_ABI:-arm64-v8a}"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-android-${ANDROID_ABI}}"
APP_NAME="OpenShell"
APK_PATH="${BUILD_DIR}/android-build/${APP_NAME}.apk"
AVD_NAME="${AVD_NAME:-}"

ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-$HOME/Library/Android/sdk}"
ADB="${ADB_EXE:-$ANDROID_SDK_ROOT/platform-tools/adb}"
EMU="${EMU_EXE:-$ANDROID_SDK_ROOT/emulator/emulator}"
# 用最新的 build-tools 里的 aapt 抽 APK 元数据（Qt 默认会生成
# org.qtproject.example.<TARGET>，跟项目里手填的 package 名可能不一致，
# 直接读 APK 自身的 manifest 最稳）。
AAPT="${AAPT_EXE:-$(ls -d "$ANDROID_SDK_ROOT"/build-tools/*/aapt 2>/dev/null | sort -V | tail -1)}"

if [[ ! -x "$ADB" ]]; then
    echo "adb not found at $ADB. Set ADB_EXE or ANDROID_SDK_ROOT." >&2
    exit 1
fi

BUILD_DIR="$BUILD_DIR" ANDROID_ABI="$ANDROID_ABI" "$ROOT_DIR/build-android.sh"

booted="$($ADB devices | awk 'NR>1 && $2=="device" {print $1; exit}')"
if [[ -z "$booted" && -n "$AVD_NAME" ]]; then
    if [[ ! -x "$EMU" ]]; then
        echo "emulator not found at $EMU. Set EMU_EXE or ANDROID_SDK_ROOT." >&2
        exit 1
    fi
    echo "Booting AVD: $AVD_NAME ..."
    "$EMU" -avd "$AVD_NAME" -no-snapshot-load >/dev/null 2>&1 &
    echo "Waiting for device..."
    "$ADB" wait-for-device
    until [[ "$($ADB shell getprop sys.boot_completed 2>/dev/null | tr -d '\r\n')" == "1" ]]; do
        sleep 2
    done
fi

if ! $ADB devices | awk 'NR>1 && $2=="device"' | grep -q .; then
    echo "No running device or emulator. Connect a phone, run an AVD, or pass AVD_NAME=..." >&2
    "$EMU" -list-avds 2>/dev/null || true
    exit 1
fi

echo "Installing $APK_PATH ..."
"$ADB" install -r -d "$APK_PATH"

# 从 APK 抽真实 package + launch activity
if [[ -z "${APP_PACKAGE:-}" || -z "${LAUNCH_ACTIVITY:-}" ]]; then
    if [[ -n "$AAPT" && -x "$AAPT" ]]; then
        badging="$("$AAPT" dump badging "$APK_PATH" 2>/dev/null)"
        APP_PACKAGE="${APP_PACKAGE:-$(awk -F"'" '/^package: name=/ {print $2; exit}' <<<"$badging")}"
        LAUNCH_ACTIVITY="${LAUNCH_ACTIVITY:-$(awk -F"'" '/^launchable-activity: name=/ {print $2; exit}' <<<"$badging")}"
    fi
fi
: "${APP_PACKAGE:?could not resolve package name}"
: "${LAUNCH_ACTIVITY:?could not resolve launch activity}"

echo "Launching $APP_PACKAGE/$LAUNCH_ACTIVITY ..."
"$ADB" shell am start -n "$APP_PACKAGE/$LAUNCH_ACTIVITY"
