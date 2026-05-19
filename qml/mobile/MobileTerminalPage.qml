import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import OpenShell 1.0

Rectangle {
    id: root

    property var session: ({})
    readonly property string sessionId: session && session.id ? session.id : ""
    property string commandText: ""

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

    function sendCommand() {
        const text = commandField.text
        if (text.length === 0) {
            return
        }
        sendText(text + "\n")
        commandField.text = ""
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
                    Layout.preferredWidth: 64
                    Layout.preferredHeight: 32
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

                Button {
                    Layout.preferredWidth: 54
                    text: "Esc"
                    enabled: root.sessionId.length > 0
                    onClicked: root.sendText("\u001b")
                }

                Button {
                    Layout.preferredWidth: 54
                    text: "Tab"
                    enabled: root.sessionId.length > 0
                    onClicked: root.sendText("\t")
                }

                Button {
                    Layout.preferredWidth: 64
                    text: "Ctrl+C"
                    enabled: root.sessionId.length > 0
                    onClicked: appController.sendSessionCtrlC(root.sessionId)
                }

                Button {
                    Layout.preferredWidth: 64
                    text: qsTr("Paste")
                    enabled: root.sessionId.length > 0
                    onClicked: root.sendText(appController.clipboardText())
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 54
            color: "#111827"
            border.color: "#1e293b"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                TextField {
                    id: commandField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Command")
                    enabled: root.sessionId.length > 0
                    onAccepted: root.sendCommand()
                }

                Button {
                    Layout.preferredWidth: 72
                    text: qsTr("Send")
                    enabled: root.sessionId.length > 0
                    onClicked: root.sendCommand()
                }
            }
        }
    }
}
