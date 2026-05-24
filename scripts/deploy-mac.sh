#!/usr/bin/env bash
# OpenShell macOS deploy: 用 macdeployqt 把 Qt 框架、QML 插件、翻译嵌进 .app，方便拷贝/打 dmg。
# 默认对 build-mac (Debug) 的 .app 进行处理，可通过 BUILD_DIR 覆盖。
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-mac}"
APP_BUNDLE="$BUILD_DIR/bin/OpenShell.app"
MAKE_DMG="${MAKE_DMG:-0}"

detect_qt_prefix() {
    if [[ -n "${QT_PREFIX:-}" && -x "$QT_PREFIX/bin/macdeployqt" ]]; then
        return
    fi
    local best=""
    if [[ -d "$HOME/Qt" ]]; then
        while IFS= read -r -d '' p; do
            if [[ -x "$p/bin/macdeployqt" ]]; then
                if [[ -z "$best" || "$p" > "$best" ]]; then
                    best="$p"
                fi
            fi
        done < <(find "$HOME/Qt" -maxdepth 2 -type d -name macos -print0 2>/dev/null)
    fi
    if [[ -n "$best" ]]; then
        QT_PREFIX="$best"
    fi
}

detect_qt_prefix

if [[ -z "${QT_PREFIX:-}" ]]; then
    echo "Qt for macOS (with macdeployqt) was not found." >&2
    echo "  Set QT_PREFIX, e.g.: export QT_PREFIX=\"\$HOME/Qt/6.11.1/macos\"" >&2
    exit 1
fi
if [[ ! -d "$APP_BUNDLE" ]]; then
    echo "App bundle not found: $APP_BUNDLE" >&2
    echo "  Build it first, e.g.:  ./build-mac.sh" >&2
    exit 1
fi

echo "Deploying with macdeployqt..."
echo "  QT_PREFIX  = $QT_PREFIX"
echo "  APP        = $APP_BUNDLE"

DEPLOY_ARGS=("-qmldir=$ROOT_DIR/qml" "-verbose=1")
if [[ "$MAKE_DMG" == "1" ]]; then
    DEPLOY_ARGS+=("-dmg")
fi

"$QT_PREFIX/bin/macdeployqt" "$APP_BUNDLE" "${DEPLOY_ARGS[@]}"
rm -rf "$APP_BUNDLE/Contents/PlugIns/sqldrivers"

if [[ -d "$ROOT_DIR/assets" ]]; then
    mkdir -p "$APP_BUNDLE/Contents/Resources/assets"
    rsync -a --delete "$ROOT_DIR/assets/" "$APP_BUNDLE/Contents/Resources/assets/"
fi

echo "Deployment finished: $APP_BUNDLE"
if [[ "$MAKE_DMG" == "1" ]]; then
    echo "DMG (next to .app): $BUILD_DIR/bin/OpenShell.dmg"
fi
