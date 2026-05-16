import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var sessions: []
    signal sessionClosed(string id)

    color: "#020617"

    Row {
        anchors.fill: parent
        anchors.leftMargin: 4
        spacing: 2

        Repeater {
            model: root.sessions
            delegate: Rectangle {
                width: tabRow.implicitWidth + 16
                height: parent.height - 4
                anchors.verticalCenter: parent.verticalCenter
                radius: 4
                color: index === 0 ? "#1e293b" : "#0f172a"
                border.color: "#1e293b"

                RowLayout {
                    id: tabRow
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 6
                    spacing: 6

                    Label {
                        text: modelData.title || qsTr("session")
                        color: "#f1f5f9"
                        font.pixelSize: 12
                    }

                    ToolButton {
                        text: "×"
                        font.pixelSize: 14
                        Layout.preferredWidth: 22
                        onClicked: root.sessionClosed(modelData.id)
                    }
                }
            }
        }
    }
}
