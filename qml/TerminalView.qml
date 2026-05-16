import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var session: ({})
    readonly property string sessionId: session && session.id ? session.id : ""

    color: "#020617"

    onSessionIdChanged: {
        // 切到新会话时，把累积缓冲一次性灌进 TextArea；之后增量靠 sessionOutput。
        output.clear()
        if (sessionId !== "") {
            const buffered = appController.sessionBuffer(sessionId)
            if (buffered.length > 0) {
                output.text = buffered
                output.cursorPosition = output.length
            }
        }
    }

    Connections {
        target: appController
        function onSessionOutput(id, chunk) {
            if (id !== root.sessionId) {
                return
            }
            output.insert(output.length, chunk)
            output.cursorPosition = output.length
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
                wrapMode: TextEdit.Wrap
                color: "#e2e8f0"
                background: Rectangle { color: "#020617" }
                font.family: "Consolas, Menlo, monospace"
                font.pixelSize: 13
                placeholderText: qsTr("Open a connection to start a session.")
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
                    font.family: "Consolas, Menlo, monospace"
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
