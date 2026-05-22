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
            color: root.fileBrowser.iconColor
        }
        Rectangle {
            x: 5
            y: 3
            width: 5
            height: 8
            radius: 1
            rotation: -18
            color: root.fileBrowser.iconAccentColor
        }
        Rectangle {
            x: 2
            y: 12
            width: 11
            height: 1
            color: root.fileBrowser.activeTextColor
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
    color: fileBrowser.panelColor
    border.color: fileBrowser.mutedBorderColor

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: qsTr("Transfer tasks")
                color: root.fileBrowser.primaryTextColor
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
                titleColor: root.fileBrowser.headerTextColor
            }

            TransferTaskList {
                Layout.fillWidth: true
                Layout.fillHeight: true
                fileBrowser: root.fileBrowser
                operation: "download"
                title: qsTr("Download")
                titleColor: root.fileBrowser.classic ? "#16a34a" : "#86efac"
            }
        }
    }
}
