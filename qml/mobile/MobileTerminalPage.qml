import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import OpenShell 1.0

Rectangle {
    id: root

    property var session: ({})
    readonly property string sessionId: session && session.id ? session.id : ""

    signal backRequested()

    color: "#020617"

    function bindScreen() {
        if (sessionId.length === 0) {
            terminal.screen = null
            return
        }
        const next = appController.sessionScreen(sessionId)
        if (next !== terminal.screen) {
            terminal.screen = next
        }
        terminal.requestFocus()
    }

    function sendText(text) {
        if (sessionId.length === 0 || text.length === 0) {
            return
        }
        appController.sendSessionInput(sessionId, text)
        terminal.requestFocus()
    }

    onSessionIdChanged: bindScreen()

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

        TerminalScreen {
            id: terminal

            Layout.fillWidth: true
            Layout.fillHeight: true
            focus: true
            activeFocusOnTab: true
            fontFamily: "Consolas"
            fontPixelSize: 13
            background: "#020617"
            cursorColor: "#38bdf8"
            onCellSizeRequested: function(cols, rows) {
                if (root.sessionId.length > 0) {
                    appController.resizeSession(root.sessionId, cols, rows)
                }
            }
            onCopySelectionRequested: function(text) {
                appController.copyTextToClipboard(text)
            }
            Component.onCompleted: root.bindScreen()
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: "#0f172a"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 6

                // 之前每个按钮固定 54-64 dp，比 Material Filled 的横向
                // padding 还小，文字直接 elide 没。改 fillWidth + 收紧 padding。
                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    leftPadding: 8
                    rightPadding: 8
                    text: "Esc"
                    enabled: root.sessionId.length > 0
                    onClicked: root.sendText("\u001b")
                }

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    leftPadding: 8
                    rightPadding: 8
                    text: "Tab"
                    enabled: root.sessionId.length > 0
                    onClicked: root.sendText("\t")
                }

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    leftPadding: 8
                    rightPadding: 8
                    text: "Ctrl+C"
                    enabled: root.sessionId.length > 0
                    onClicked: appController.sendSessionCtrlC(root.sessionId)
                }

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    leftPadding: 8
                    rightPadding: 8
                    text: qsTr("Paste")
                    enabled: root.sessionId.length > 0
                    onClicked: root.sendText(appController.clipboardText())
                }
            }
        }

        // 之前底部还有一行 "Command 输入框 + Send"，移动端直接点 terminal
        // 就能弹软键盘逐字符发到 SSH，那一行的"前置编辑再回车整段发"心智
        // 在 vim / nano / 交互式 prompt 下走不通，砍掉留给 terminal 更多
        // 垂直空间。特殊键还在上面那条工具栏里（Esc / Tab / Ctrl+C / Paste）。
    }
}
