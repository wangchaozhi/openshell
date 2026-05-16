import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var session: ({})
    readonly property string sessionId: session && session.id ? session.id : ""
    property int terminalCols: 0
    property int terminalRows: 0

    color: "#020617"

    function syncTerminalSize() {
        if (sessionId === "" || output.width <= 0 || output.height <= 0) {
            return
        }
        const charWidth = Math.max(7, output.fontMetrics.advanceWidth("W"))
        const lineHeight = Math.max(12, output.fontMetrics.height)
        const cols = Math.max(20, Math.floor(output.width / charWidth))
        const rows = Math.max(4, Math.floor(output.height / lineHeight))
        if (cols === terminalCols && rows === terminalRows) {
            return
        }
        terminalCols = cols
        terminalRows = rows
        appController.resizeSession(sessionId, cols, rows)
    }

    function sendTerminalKey(event) {
        if (sessionId === "") {
            return false
        }

        if (event.modifiers & Qt.ControlModifier) {
            if (event.key === Qt.Key_C) {
                appController.sendSessionInput(sessionId, "\u0003")
                return true
            }
            if (event.key === Qt.Key_D) {
                appController.sendSessionInput(sessionId, "\u0004")
                return true
            }
            if (event.key === Qt.Key_L) {
                appController.sendSessionInput(sessionId, "\u000c")
                return true
            }
            if (event.key === Qt.Key_Z) {
                appController.sendSessionInput(sessionId, "\u001a")
                return true
            }
        }

        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            appController.sendSessionInput(sessionId, "\r")
            return true
        }
        if (event.key === Qt.Key_Backspace) {
            appController.sendSessionInput(sessionId, "\u007f")
            return true
        }
        if (event.key === Qt.Key_Delete) {
            appController.sendSessionInput(sessionId, "\u001b[3~")
            return true
        }
        if (event.key === Qt.Key_Tab) {
            appController.sendSessionInput(sessionId, "\t")
            return true
        }
        if (event.key === Qt.Key_Up) {
            appController.sendSessionInput(sessionId, "\u001b[A")
            return true
        }
        if (event.key === Qt.Key_Down) {
            appController.sendSessionInput(sessionId, "\u001b[B")
            return true
        }
        if (event.key === Qt.Key_Right) {
            appController.sendSessionInput(sessionId, "\u001b[C")
            return true
        }
        if (event.key === Qt.Key_Left) {
            appController.sendSessionInput(sessionId, "\u001b[D")
            return true
        }

        if (event.text && event.text.length > 0) {
            appController.sendSessionInput(sessionId, event.text)
            return true
        }

        return false
    }

    onSessionIdChanged: {
        // 切到新会话时，把累积缓冲一次性灌进 TextArea；之后增量靠 sessionOutput。
        output.clear()
        if (sessionId !== "") {
            const buffered = appController.sessionBuffer(sessionId)
            if (buffered.length > 0) {
                output.text = buffered
                output.cursorPosition = output.length
            }
            output.forceActiveFocus()
            Qt.callLater(syncTerminalSize)
        }
    }

    Connections {
        target: appController
        function onSessionOutput(id, chunk) {
            if (id !== root.sessionId) {
                return
            }
            output.text = appController.sessionBuffer(root.sessionId)
            output.cursorPosition = output.length
            resizeDebounce.restart()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 0
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            color: "#0f172a"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12

                Label {
                    text: root.session && root.session.title ? root.session.title : qsTr("(no session)")
                    color: "#cbd5f5"
                    font.pixelSize: 12
                    Layout.fillWidth: true
                }

                Label {
                    text: root.session && root.session.status ? root.session.status : ""
                    color: {
                        const s = root.session && root.session.status
                        if (s === "connected") return "#34d399"
                        if (s === "connecting") return "#fbbf24"
                        if (s === "error") return "#f87171"
                        return "#94a3b8"
                    }
                    font.pixelSize: 11
                }
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            TextArea {
                id: output
                readOnly: true
                focus: true
                activeFocusOnTab: true
                selectByMouse: false
                cursorVisible: false
                wrapMode: TextEdit.Wrap
                color: "#e2e8f0"
                background: Rectangle { color: "#020617" }
                font.family: "Consolas"
                font.pixelSize: 13
                placeholderText: qsTr("Open a connection to start a session.")
                Component.onCompleted: forceActiveFocus()
                onWidthChanged: resizeDebounce.restart()
                onHeightChanged: resizeDebounce.restart()
                FontMetrics {
                    id: outputFontMetrics
                    font: output.font
                }
                readonly property var fontMetrics: outputFontMetrics
                Timer {
                    id: resizeDebounce
                    interval: 120
                    repeat: false
                    onTriggered: root.syncTerminalSize()
                }
                Rectangle {
                    width: Math.max(8, output.cursorRectangle.width)
                    height: Math.max(output.font.pixelSize + 2, output.cursorRectangle.height)
                    x: output.cursorRectangle.x
                    y: output.cursorRectangle.y
                    color: "#38bdf8"
                    opacity: terminalCursorBlink.visiblePhase ? 0.85 : 0
                    visible: root.sessionId !== "" && output.activeFocus
                    z: 10

                    Timer {
                        id: terminalCursorBlink
                        property bool visiblePhase: true
                        interval: 530
                        repeat: true
                        running: root.sessionId !== "" && output.activeFocus
                        onTriggered: visiblePhase = !visiblePhase
                        onRunningChanged: visiblePhase = true
                    }
                }
                TapHandler {
                    onTapped: {
                        output.cursorPosition = output.length
                        output.forceActiveFocus()
                    }
                }
                Keys.onPressed: function(event) {
                    output.cursorPosition = output.length
                    if (root.sendTerminalKey(event)) {
                        event.accepted = true
                    }
                }
                onCursorPositionChanged: {
                    if (cursorPosition !== length) {
                        cursorPosition = length
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            color: "#0f172a"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12

                Label {
                    text: "$"
                    color: "#38bdf8"
                    font.family: "Consolas"
                    font.pixelSize: 13
                }

                TextField {
                    id: commandField
                    Layout.fillWidth: true
                    enabled: root.sessionId !== ""
                    placeholderText: enabled
                                     ? qsTr("Type a command and press Enter")
                                     : qsTr("(no session)")
                    background: null
                    color: "#e2e8f0"
                    font.family: "Consolas, Menlo, monospace"
                    Keys.onTabPressed: function(event) {
                        if (root.sessionId === "") {
                            return
                        }
                        appController.sendSessionInput(root.sessionId, "\t")
                        event.accepted = true
                    }
                    onAccepted: {
                        if (root.sessionId === "") {
                            return
                        }
                        appController.sendSessionInput(root.sessionId, text + "\n")
                        text = ""
                    }
                }
            }
        }
    }
}
