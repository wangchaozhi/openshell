pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: localWindow

    required property var fileBrowser

    property bool pendingSystemMove: false

    onFrameSwapped: {
        if (pendingSystemMove) {
            pendingSystemMove = false
            startSystemMove()
        }
    }

    width: 720
    height: 520
    minimumWidth: 420
    minimumHeight: 300
    visible: false
    title: qsTr("Local") + " - " + fileBrowser.localPath
    color: fileBrowser.panelColor

    Rectangle {
        id: localWindowContent
        anchors.fill: parent
        color: localWindow.fileBrowser.panelColor

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: qsTr("Local")
                    color: localWindow.fileBrowser.secondaryTextColor
                    font.bold: true
                    font.pixelSize: 12
                }
                ToolButton {
                    implicitWidth: 30
                    implicitHeight: 24
                    enabled: localWindow.fileBrowser.localBackStack.length > 0
                    contentItem: ArrowIcon {
                        anchors.centerIn: parent
                        direction: "left"
                        color: localWindow.fileBrowser.iconColor
                        opacity: parent.enabled ? 1 : 0.35
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Back")
                    onClicked: localWindow.fileBrowser.goLocalBack()
                }
                ToolButton {
                    implicitWidth: 30
                    implicitHeight: 24
                    enabled: localWindow.fileBrowser.localForwardStack.length > 0
                    contentItem: ArrowIcon {
                        anchors.centerIn: parent
                        direction: "right"
                        color: localWindow.fileBrowser.iconColor
                        opacity: parent.enabled ? 1 : 0.35
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Forward")
                    onClicked: localWindow.fileBrowser.goLocalForward()
                }
                ToolButton {
                    implicitWidth: 30
                    implicitHeight: 24
                    contentItem: ArrowIcon {
                        anchors.centerIn: parent
                        direction: "up"
                        color: localWindow.fileBrowser.iconColor
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Parent folder")
                    onClicked: localWindow.fileBrowser.localParent()
                }
                ToolButton {
                    implicitWidth: 30
                    implicitHeight: 24
                    contentItem: RefreshIcon {
                        anchors.centerIn: parent
                        color: localWindow.fileBrowser.iconColor
                        maskColor: localWindow.fileBrowser.panelColor
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Refresh")
                    onClicked: localWindow.fileBrowser.refreshLocal()
                }
            }

            TextField {
                Layout.fillWidth: true
                text: localWindow.fileBrowser.localPath
                selectByMouse: true
                color: localWindow.fileBrowser.primaryTextColor
                font.pixelSize: 11
                background: Rectangle {
                    color: localWindow.fileBrowser.surfaceColor
                    border.color: localWindow.fileBrowser.borderColor
                    radius: 3
                }
                onAccepted: {
                    localWindow.fileBrowser.enterLocal(text)
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 22
                color: localWindow.fileBrowser.surfaceColor
                Item {
                    id: detachedLocalHeaderInner
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    Repeater {
                        model: localWindow.fileBrowser.localColumnOrder
                        delegate: HeaderCell {
                            required property string modelData
                            required property int index
                            panelName: "local"
                            fileBrowser: localWindow.fileBrowser
                            colId: modelData
                            naturalIndex: index
                            parentWidth: detachedLocalHeaderInner.width
                        }
                    }
                }
            }

            ListView {
                id: detachedLocalList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: localWindow.fileBrowser.localEntries
                reuseItems: true
                cacheBuffer: 900

                delegate: Rectangle {
                    id: detachedLocalRow
                    required property var modelData
                    required property int index
                    width: detachedLocalList.width
                    height: 26
                    color: detachedLocalMouse.containsMouse ? localWindow.fileBrowser.hoverColor
                                                            : localWindow.fileBrowser.panelColor

                    Item {
                        id: detachedLocalRowInner
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        Repeater {
                            model: localWindow.fileBrowser.localColumnOrder
                            delegate: DataCell {
                                required property string modelData
                                required property int index
                                panelName: "local"
                                fileBrowser: localWindow.fileBrowser
                                colId: modelData
                                naturalIndex: index
                                parentWidth: detachedLocalRowInner.width
                                entry: detachedLocalRow.modelData
                            }
                        }
                    }

                    MouseArea {
                        id: detachedLocalMouse
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        hoverEnabled: true
                        onDoubleClicked: {
                            if (detachedLocalRow.modelData.isDir) {
                                localWindow.fileBrowser.enterLocal(detachedLocalRow.modelData.path)
                            }
                        }
                        onClicked: function(mouse) {
                            if (mouse.button === Qt.RightButton) {
                                detachedLocalMenu.path = detachedLocalRow.modelData.path
                                detachedLocalMenu.popup()
                            }
                        }
                    }
                }

                Menu {
                    id: detachedLocalMenu
                    property string path: ""
                    MenuItem {
                        text: qsTr("Upload to Remote")
                        enabled: localWindow.fileBrowser.connectionId !== "" && detachedLocalMenu.path.length > 0
                        onTriggered: localWindow.fileBrowser.uploadLocalPath(detachedLocalMenu.path)
                    }
                }

                ScrollBar.vertical: ScrollBar {}
            }
        }
    }

    onClosing: function(close) {
        close.accepted = true
        destroy()
    }
    Component.onDestruction: {
        localWindow.fileBrowser.localDetachedWindow = null
        localWindow.fileBrowser.updateDetachedPaneState()
    }
}
