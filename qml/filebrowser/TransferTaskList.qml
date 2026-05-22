import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    required property var fileBrowser
    required property string operation
    required property string title
    required property color titleColor

    color: fileBrowser.surfaceColor
    radius: 4
    border.color: fileBrowser.borderColor

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: root.title
                color: root.titleColor
                font.pixelSize: 12
                font.bold: true
                Layout.fillWidth: true
            }

            Label {
                text: String(root.fileBrowser.transferTasksFor(root.operation).length)
                color: root.fileBrowser.mutedTextColor
                font.pixelSize: 10
            }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.fileBrowser.transferTasksFor(root.operation)
            spacing: 7

            delegate: Rectangle {
                required property var modelData

                width: ListView.view.width
                height: 64
                color: root.operation === "download"
                       && downloadMouse.containsMouse
                       && root.fileBrowser.downloadOpenPath(modelData).length > 0
                       ? (root.fileBrowser.classic ? "#dcfce7" : "#10251c")
                       : root.fileBrowser.panelColor
                radius: 4
                border.color: modelData.status === "failed" ? root.fileBrowser.theme.danger : root.fileBrowser.borderColor

                MouseArea {
                    id: downloadMouse
                    anchors.fill: parent
                    enabled: root.operation === "download"
                    hoverEnabled: enabled
                    acceptedButtons: Qt.LeftButton
                    onDoubleClicked: root.fileBrowser.openDownloadedFolder(modelData)
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            text: root.fileBrowser.shortPath(modelData.path)
                            color: root.fileBrowser.primaryTextColor
                            font.pixelSize: 11
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }

                        Label {
                            text: modelData.status === "failed"
                                  ? qsTr("Failed")
                                  : modelData.status === "done"
                                    ? qsTr("Done")
                                    : qsTr("%1%").arg(root.fileBrowser.transferPercent(modelData))
                            color: modelData.status === "failed" ? root.fileBrowser.theme.danger
                                  : modelData.status === "done" ? root.fileBrowser.theme.success
                                  : root.fileBrowser.mutedTextColor
                            font.pixelSize: 11
                        }
                    }

                    ProgressBar {
                        id: transferProgress
                        Layout.fillWidth: true
                        Layout.preferredHeight: 6
                        from: 0
                        to: modelData.total > 0 ? modelData.total : 1
                        value: modelData.status === "done" ? (modelData.total > 0 ? modelData.total : 1)
                              : modelData.total > 0 ? modelData.done
                              : 0
                        indeterminate: modelData.status === "running" && modelData.total <= 0

                        background: Rectangle {
                            implicitHeight: 6
                            radius: 3
                            color: root.fileBrowser.theme.classic ? "#e2e8f0" : root.fileBrowser.theme.surfaceRaised
                            border.width: 1
                            border.color: root.fileBrowser.borderColor
                        }

                        contentItem: Item {
                            implicitHeight: 6

                            Rectangle {
                                width: transferProgress.indeterminate
                                       ? parent.width * 0.36
                                       : parent.width * transferProgress.visualPosition
                                height: parent.height
                                radius: 3
                                color: modelData.status === "failed" ? root.fileBrowser.theme.danger
                                      : modelData.status === "done" ? root.fileBrowser.theme.success
                                      : root.fileBrowser.theme.focus
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: modelData.status === "running"
                              ? root.fileBrowser.formatSpeed(modelData.speed)
                              : root.operation === "download"
                                ? (modelData.localPath || modelData.message || root.fileBrowser.formatFileSize(modelData.done))
                                : (modelData.message || root.fileBrowser.formatFileSize(modelData.done))
                        color: root.fileBrowser.mutedTextColor
                        font.pixelSize: 10
                        elide: Text.ElideMiddle
                    }
                }
            }

            ScrollBar.vertical: ScrollBar {}
        }
    }
}
