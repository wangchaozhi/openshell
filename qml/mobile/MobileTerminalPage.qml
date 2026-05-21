import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import OpenShell 1.0

Rectangle {
    id: root

    property var session: ({})
    property bool pageActive: false
    property bool ctrlModifier: false
    property bool altModifier: false
    property bool keyboardRequested: false
    readonly property bool keyboardVisible: pageActive && keyboardRequested
    readonly property real estimatedKeyboardInset: keyboardVisible ? root.height * 0.42 : 0
    readonly property int terminalKeyboardGap: keyboardVisible ? 18 : 0
    readonly property real terminalBottomInset: keyboardVisible ? Math.max(keyboardInset, estimatedKeyboardInset) + terminalKeyboardGap : 0
    readonly property real keyboardInset: {
        if (!keyboardVisible) {
            return 0
        }
        const rect = Qt.inputMethod.keyboardRectangle
        if (!rect || rect.height <= 0 || rect.y <= 0) {
            return 0
        }
        // Android 有时会先缩小 Qt 窗口，有时键盘覆盖在窗口上；只补实际
        // 重叠的高度，避免第二次弹出时重复避让把工具栏顶出屏幕。
        const keyboardTop = root.mapFromGlobal(Qt.point(rect.x, rect.y)).y
        const overlap = root.height - keyboardTop
        if (overlap > 16) {
            return overlap
        }
        // 某些 Android/Qt 组合返回的 keyboardRectangle 坐标系和 Item 坐标
        // 不一致，overlap 会误算成 0。这里按可见键盘高度兜底，确保终端
        // 输出区域一定收在软键盘上方。
        return Math.min(rect.height, root.height * 0.42)
    }
    readonly property string sessionId: session && session.id ? session.id : ""
    readonly property bool sessionConnected: session && session.status === "connected"

    signal backRequested()

    color: "#020617"

    component SpecialKeyButton: Button {
        property bool active: false

        Layout.fillWidth: true
        Layout.preferredHeight: 34
        leftPadding: 2
        rightPadding: 2
        focusPolicy: Qt.NoFocus
        highlighted: active
        font.pixelSize: 12
    }

    function bindScreen() {
        if (sessionId.length === 0) {
            terminal.screen = null
            return
        }
        const next = appController.sessionScreen(sessionId)
        if (next !== terminal.screen) {
            terminal.screen = next
        }
        if (root.pageActive && terminal.screen !== null) {
            keyboardProxy.activate()
        }
    }

    function sendText(text) {
        if (sessionId.length === 0 || text.length === 0) {
            return
        }
        appController.sendSessionInput(sessionId, applyPendingModifiers(text))
        keyboardProxy.activate()
    }

    function sendRaw(text) {
        if (sessionId.length === 0 || text.length === 0) {
            return
        }
        ctrlModifier = false
        altModifier = false
        appController.sendSessionInput(sessionId, text)
        keyboardProxy.activate()
    }

    function sendBackspace() {
        if (sessionId.length === 0) {
            return
        }
        sendRaw("\u007f")
    }

    function controlChar(ch) {
        if (!ch || ch.length === 0) {
            return ""
        }
        const c = ch.charAt(0).toUpperCase()
        const code = c.charCodeAt(0)
        if (code >= 65 && code <= 90) {
            return String.fromCharCode(code - 64)
        }
        if (c === " " || c === "@") return "\u0000"
        if (c === "[") return "\u001b"
        if (c === "\\") return "\u001c"
        if (c === "]") return "\u001d"
        if (c === "^") return "\u001e"
        if (c === "_") return "\u001f"
        if (c === "?") return "\u007f"
        return ch
    }

    function applyPendingModifiers(text) {
        if (!ctrlModifier && !altModifier) {
            return text
        }
        let output = text
        if (ctrlModifier && text.length > 0) {
            output = controlChar(text.charAt(0)) + text.slice(1)
        }
        if (altModifier) {
            output = "\u001b" + output
        }
        ctrlModifier = false
        altModifier = false
        return output
    }

    function toggleCtrl() {
        ctrlModifier = !ctrlModifier
        keyboardProxy.activate()
    }

    function toggleAlt() {
        altModifier = !altModifier
        keyboardProxy.activate()
    }

    function toggleKeyboard() {
        if (!pageActive || sessionId.length === 0) {
            return
        }
        if (keyboardRequested && keyboardProxy.activeFocus) {
            ctrlModifier = false
            altModifier = false
            keyboardRequested = false
            Qt.inputMethod.hide()
            return
        }
        keyboardProxy.activate()
    }

    onSessionIdChanged: bindScreen()
    onKeyboardVisibleChanged: {
        Qt.callLater(function() {
            terminal.scrollToBottom()
        })
    }
    onPageActiveChanged: {
        if (pageActive) {
            bindScreen()
        } else {
            ctrlModifier = false
            altModifier = false
            keyboardRequested = false
            Qt.inputMethod.hide()
        }
    }

    Connections {
        target: appController
        function onSessionsChanged() {
            root.bindScreen()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            color: "#0f172a"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                Button {
                    Layout.preferredHeight: 36
                    text: qsTr("Back")
                    onClicked: root.backRequested()
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Label {
                        Layout.fillWidth: true
                        text: root.session && root.session.title ? root.session.title : qsTr("No session")
                        color: "#f8fafc"
                        font.pixelSize: 13
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.session && root.session.status ? root.session.status : ""
                        color: "#94a3b8"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 86
            color: "#0f172a"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 5

                    // 放在终端上方，避免 Android 软键盘覆盖底部工具栏。
                    SpecialKeyButton {
                        text: "Esc"
                        enabled: root.sessionId.length > 0
                        onClicked: root.sendRaw("\u001b")
                    }

                    SpecialKeyButton {
                        text: "Tab"
                        enabled: root.sessionId.length > 0
                        onClicked: root.sendRaw("\t")
                    }

                    SpecialKeyButton {
                        text: "Ctrl"
                        active: root.ctrlModifier
                        enabled: root.sessionId.length > 0
                        onClicked: root.toggleCtrl()
                    }

                    SpecialKeyButton {
                        text: "Alt"
                        active: root.altModifier
                        enabled: root.sessionId.length > 0
                        onClicked: root.toggleAlt()
                    }

                    SpecialKeyButton {
                        text: "←"
                        enabled: root.sessionId.length > 0
                        onClicked: root.sendRaw("\u001b[D")
                    }

                    SpecialKeyButton {
                        text: "↓"
                        enabled: root.sessionId.length > 0
                        onClicked: root.sendRaw("\u001b[B")
                    }

                    SpecialKeyButton {
                        text: "↑"
                        enabled: root.sessionId.length > 0
                        onClicked: root.sendRaw("\u001b[A")
                    }

                    SpecialKeyButton {
                        text: "→"
                        enabled: root.sessionId.length > 0
                        onClicked: root.sendRaw("\u001b[C")
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 5

                    SpecialKeyButton {
                        text: "Enter"
                        enabled: root.sessionId.length > 0
                        onClicked: root.sendRaw("\r")
                    }

                    SpecialKeyButton {
                        text: "Bksp"
                        enabled: root.sessionId.length > 0
                        onClicked: root.sendBackspace()
                    }

                    SpecialKeyButton {
                        text: "Del"
                        enabled: root.sessionId.length > 0
                        onClicked: root.sendRaw("\u001b[3~")
                    }

                    SpecialKeyButton {
                        text: "Home"
                        enabled: root.sessionId.length > 0
                        onClicked: root.sendRaw("\u001b[H")
                    }

                    SpecialKeyButton {
                        text: "End"
                        enabled: root.sessionId.length > 0
                        onClicked: root.sendRaw("\u001b[F")
                    }

                    SpecialKeyButton {
                        text: "PgUp"
                        enabled: root.sessionId.length > 0
                        onClicked: root.sendRaw("\u001b[5~")
                    }

                    SpecialKeyButton {
                        text: "PgDn"
                        enabled: root.sessionId.length > 0
                        onClicked: root.sendRaw("\u001b[6~")
                    }

                    SpecialKeyButton {
                        text: qsTr("Paste")
                        enabled: root.sessionId.length > 0
                        onClicked: root.sendRaw(appController.clipboardText())
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            TerminalScreen {
                id: terminal

                anchors.fill: parent
                anchors.bottomMargin: root.terminalBottomInset
                focus: false
                activeFocusOnTab: false
                fontFamily: "Consolas"
                fontPixelSize: 13
                background: "#020617"
                cursorColor: "#38bdf8"
                cursorBlinking: root.sessionConnected
                onCellSizeRequested: function(cols, rows) {
                    if (root.sessionId.length > 0) {
                        appController.resizeSession(root.sessionId, cols, rows)
                        Qt.callLater(function() {
                            terminal.scrollToBottom()
                        })
                    }
                }
                onCopySelectionRequested: function(text) {
                    appController.copyTextToClipboard(text)
                }
                Component.onCompleted: root.bindScreen()
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                preventStealing: false
                propagateComposedEvents: true
                onClicked: (mouse) => {
                    root.toggleKeyboard()
                    mouse.accepted = false
                }
            }

            TextInput {
                id: keyboardProxy

                readonly property string sentinel: " "
                anchors.left: parent.left
                anchors.top: parent.top
                width: 1
                height: 1
                opacity: 0.01
                text: sentinel
                cursorPosition: text.length
                activeFocusOnPress: false
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                                  | Qt.ImhSensitiveData | Qt.ImhNoExtractedText
                                  | Qt.ImhNoEditMenu | Qt.ImhNoTextHandles

                function activate() {
                    if (!root.pageActive || root.sessionId.length === 0) {
                        return
                    }
                    forceActiveFocus()
                    reset()
                    root.keyboardRequested = true
                    Qt.inputMethod.show()
                }

                function reset() {
                    if (text !== sentinel) {
                        text = sentinel
                    }
                    cursorPosition = text.length
                }

                onTextEdited: {
                    if (!root.pageActive || root.sessionId.length === 0) {
                        reset()
                        return
                    }
                    if (text.length === 0) {
                        root.sendBackspace()
                        reset()
                        return
                    }
                    if (text.length > sentinel.length) {
                        const committed = text.slice(sentinel.length)
                        reset()
                        root.sendText(committed)
                        return
                    }
                    reset()
                }

                Keys.onReturnPressed: (event) => {
                    root.sendText("\r")
                    event.accepted = true
                }
                Keys.onEnterPressed: (event) => {
                    root.sendText("\r")
                    event.accepted = true
                }
                Keys.onTabPressed: (event) => {
                    root.sendText("\t")
                    event.accepted = true
                }
                Keys.onEscapePressed: (event) => {
                    root.sendText("\u001b")
                    event.accepted = true
                }
            }
        }

        // 之前底部还有一行 "Command 输入框 + Send"，移动端直接点 terminal
        // 就能弹软键盘逐字符发到 SSH，那一行的"前置编辑再回车整段发"心智
        // 在 vim / nano / 交互式 prompt 下走不通，砍掉留给 terminal 更多
        // 垂直空间。特殊键还在上面那条工具栏里（Esc / Tab / Ctrl+C / Paste）。
    }
}
