import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    required property var fileBrowser
    required property string operation
    required property string title
    required property color titleColor

    color: "#0f172a"
    radius: 4
    border.color: "#1e293b"

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
                color: "#64748b"
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
                       ? "#10251c"
                       : "#020617"
                radius: 4
                border.color: modelData.status === "failed" ? "#7f1d1d" : "#1e293b"

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
                            color: "#dbeafe"
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
                            color: modelData.status === "failed" ? "#fca5a5" : "#94a3b8"
                            font.pixelSize: 11
                        }
                    }

                    ProgressBar {
                        Layout.fillWidth: true
                        from: 0
                        to: modelData.total > 0 ? modelData.total : 1
                        value: modelData.total > 0 ? modelData.done : 0
                        indeterminate: modelData.status === "running" && modelData.total <= 0
                    }

                    Label {
                        Layout.fillWidth: true
                        text: modelData.status === "running"
                              ? root.fileBrowser.formatSpeed(modelData.speed)
                              : root.operation === "download"
                                ? (modelData.localPath || modelData.message || root.fileBrowser.formatFileSize(modelData.done))
                                : (modelData.message || root.fileBrowser.formatFileSize(modelData.done))
                        color: "#94a3b8"
                        font.pixelSize: 10
                        elide: Text.ElideMiddle
                    }
                }
            }

            ScrollBar.vertical: ScrollBar {}
        }
    }
}
