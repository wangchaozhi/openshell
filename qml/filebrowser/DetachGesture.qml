import QtQuick

// 窗格把手的拆出手势：长按一小会儿即“就位”(armed)，
// 期间允许移动；就位后拖动越过阈值才真正触发拆出。
// 放在标题/按钮内部并填满它，手势作用于宿主控件。
Item {
    id: gesture
    anchors.fill: parent

    property bool armed: false
    property bool holdReady: false
    property bool started: false
    signal detachTriggered()

    function resetIfIdle() {
        if (!dragHandler.active && !gesture.started) {
            armTimer.stop()
            gesture.holdReady = false
            gesture.armed = false
        }
    }

    Timer {
        id: armTimer
        interval: 250
        onTriggered: {
            gesture.holdReady = true
            gesture.armed = true
        }
    }

    TapHandler {
        acceptedButtons: Qt.LeftButton
        onPressedChanged: {
            if (pressed) {
                gesture.holdReady = false
                gesture.started = false
                armTimer.restart()
            } else {
                Qt.callLater(gesture.resetIfIdle)
            }
        }
    }

    DragHandler {
        id: dragHandler
        target: null
        dragThreshold: 0
        onActiveChanged: {
            if (!active) {
                armTimer.stop()
                gesture.holdReady = false
                gesture.started = false
                gesture.armed = false
            }
        }
        onTranslationChanged: {
            if (!active || !gesture.holdReady || gesture.started) {
                return
            }
            // 长按就位后，开始拖动才真正拆出窗口；
            // 拆出后由系统窗口管理器接管，窗口跟随鼠标。
            if (Math.hypot(translation.x, translation.y) <= 6) {
                return
            }
            gesture.started = true
            gesture.detachTriggered()
        }
    }
}
