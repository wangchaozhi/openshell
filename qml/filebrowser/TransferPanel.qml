import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    required property var fileBrowser

    component BroomIcon: Item {
        implicitWidth: 15
        implicitHeight: 15
        Rectangle {
            x: 3
            y: 9
            width: 9
            height: 4
            radius: 1
            color: "#93c5fd"
        }
        Rectangle {
            x: 5
            y: 3
            width: 5
            height: 8
            radius: 1
            rotation: -18
            color: "#60a5fa"
        }
        Rectangle {
            x: 2
            y: 12
            width: 11
            height: 1
            color: "#bfdbfe"
        }
    }

    visible: fileBrowser.transferPanelOpen && fileBrowser.transferTasks.length > 0
    z: 30
    width: Math.min(680, parent.width - 16)
    height: Math.min(320, parent.height - 48)
    anchors.top: parent.top
    anchors.topMargin: 38
    anchors.right: parent.right
    anchors.rightMargin: 8
    radius: 4
    color: "#020617"
    border.color: "#334155"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: qsTr("Transfer tasks")
                color: "#dbeafe"
                font.bold: true
                font.pixelSize: 12
                Layout.fillWidth: true
            }

            ToolButton {
                implicitWidth: 24
                implicitHeight: 22
                contentItem: BroomIcon {
                    anchors.centerIn: parent
                }
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Clear finished tasks")
                onClicked: root.fileBrowser.clearFinishedTransfers()
            }

            ToolButton {
                implicitWidth: 24
                implicitHeight: 22
                text: "x"
                onClicked: root.fileBrowser.transferPanelOpen = false
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            TransferTaskList {
                Layout.fillWidth: true
                Layout.fillHeight: true
                fileBrowser: root.fileBrowser
                operation: "upload"
                title: qsTr("Upload")
                titleColor: "#93c5fd"
            }

            TransferTaskList {
                Layout.fillWidth: true
                Layout.fillHeight: true
                fileBrowser: root.fileBrowser
                operation: "download"
                title: qsTr("Download")
                titleColor: "#86efac"
            }
        }
    }
}
