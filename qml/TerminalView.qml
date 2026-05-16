import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var session: ({})

    color: "#020617"

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
                    color: "#fbbf24"
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
                text: root.session && root.session.lastMessage
                      ? "[" + (root.session.status || "?") + "] " + root.session.lastMessage + "\n"
                      : qsTr("Open a connection to start a session.")
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
                    placeholderText: qsTr("Type a command and press Enter (stub)")
                    background: null
                    color: "#e2e8f0"
                    font.family: "Consolas, Menlo, monospace"
                    onAccepted: {
                        output.append("$ " + text + "\n")
                        output.append("(stub) command not sent — wire SSH backend\n")
                        text = ""
                    }
                }
            }
        }
    }
}
