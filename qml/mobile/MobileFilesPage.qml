import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property bool hasSession: false

    color: "#0b1220"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Label {
            Layout.fillWidth: true
            text: qsTr("Files")
            color: "#f8fafc"
            font.pixelSize: 22
            font.bold: true
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 8
            color: "#111827"
            border.color: "#1e293b"

            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(parent.width - 32, 320)
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    text: root.hasSession ? qsTr("Mobile SFTP page is planned.") : qsTr("Open a session first")
                    color: "#e2e8f0"
                    font.pixelSize: 15
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("The first mobile version will use a single-column remote file browser.")
                    color: "#94a3b8"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
