import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var sessions: []
    property string activeSessionId: ""
    property string activeView: "terminal"

    signal sessionActivated(string id)
    signal sessionClosed(string id)
    signal systemInfoActivated()

    color: "#020617"

    component CloseIcon: Item {
        implicitWidth: 12
        implicitHeight: 12
        Rectangle {
            anchors.centerIn: parent
            width: 13
            height: 2
            radius: 1
            rotation: 45
            color: "#cbd5e1"
        }
        Rectangle {
            anchors.centerIn: parent
            width: 13
            height: 2
            radius: 1
            rotation: -45
            color: "#cbd5e1"
        }
    }

    Row {
        anchors.fill: parent
        anchors.leftMargin: 4
        spacing: 2

        Rectangle {
            width: systemTabRow.implicitWidth + 18
            height: parent.height - 4
            anchors.verticalCenter: parent.verticalCenter
            radius: 4
            visible: root.activeSessionId !== ""
            color: root.activeView === "system" ? "#1e293b" : "#0f172a"
            border.color: root.activeView === "system" ? "#38bdf8" : "#1e293b"

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.systemInfoActivated()
            }

            RowLayout {
                id: systemTabRow
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10

                Label {
                    text: qsTr("System Info")
                    color: root.activeView === "system" ? "#f1f5f9" : "#cbd5f5"
                    font.bold: root.activeView === "system"
                    font.pixelSize: 12
                }
            }
        }

        Repeater {
            model: root.sessions
            delegate: Rectangle {
                id: tab
                width: tabRow.implicitWidth + 16
                height: parent.height - 4
                anchors.verticalCenter: parent.verticalCenter
                radius: 4

                readonly property bool isActive: modelData.id === root.activeSessionId

                color: isActive && root.activeView === "terminal" ? "#1e293b" : "#0f172a"
                border.color: isActive && root.activeView === "terminal" ? "#38bdf8" : "#1e293b"
                border.width: 1

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.sessionActivated(modelData.id)
                }

                RowLayout {
                    id: tabRow
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 6
                    spacing: 6

                    Label {
                        text: modelData.title || qsTr("session")
                        color: tab.isActive && root.activeView === "terminal" ? "#f1f5f9" : "#cbd5f5"
                        font.pixelSize: 12
                    }

                    ToolButton {
                        Layout.preferredWidth: 22
                        Layout.preferredHeight: 22
                        contentItem: CloseIcon {
                            anchors.centerIn: parent
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Close session")
                        onClicked: root.sessionClosed(modelData.id)
                    }
                }
            }
        }
    }
}
