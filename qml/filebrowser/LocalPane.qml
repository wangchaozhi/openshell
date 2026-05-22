pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".." as OpenShellComponents

// 文件浏览器的“本地”窗格。从 FileBrowser.qml 内联块拆出，逻辑全部走
// fileBrowser（即 FileBrowser 根对象）暴露的属性与函数。
Rectangle {
    id: pane

    required property var fileBrowser

    color: fileBrowser.panelColor
    radius: 4

    OpenShellComponents.MarchingAntsBorder {
        anchors.fill: parent
        z: 50
        cornerRadius: parent.radius
        active: pane.fileBrowser.localDetaching
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        RowLayout {
            Layout.fillWidth: true

            Label {
                id: localTitleLabel
                text: qsTr("Local")
                color: pane.fileBrowser.secondaryTextColor
                font.bold: true
                font.pixelSize: 12

                DetachGesture {
                    onArmedChanged: pane.fileBrowser.localDetaching = armed
                    onDetachTriggered: pane.fileBrowser.detachLocalPane(localTitleLabel)
                }
            }

            ToolButton {
                id: localDetachButton
                implicitWidth: 30
                implicitHeight: 24
                contentItem: DetachIcon {
                    anchors.centerIn: parent
                    color: pane.fileBrowser.iconColor
                    accentColor: pane.fileBrowser.iconAccentColor
                }
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Detach Window")

                DetachGesture {
                    onArmedChanged: pane.fileBrowser.localDetaching = armed
                    onDetachTriggered: pane.fileBrowser.detachLocalPane(localDetachButton)
                }
            }

            ToolButton {
                implicitWidth: 30
                implicitHeight: 24
                enabled: pane.fileBrowser.localBackStack.length > 0
                contentItem: ArrowIcon {
                    anchors.centerIn: parent
                    direction: "left"
                    color: pane.fileBrowser.iconColor
                    opacity: parent.enabled ? 1 : 0.35
                }
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Back")
                onClicked: pane.fileBrowser.goLocalBack()
            }

            ToolButton {
                implicitWidth: 30
                implicitHeight: 24
                enabled: pane.fileBrowser.localForwardStack.length > 0
                contentItem: ArrowIcon {
                    anchors.centerIn: parent
                    direction: "right"
                    color: pane.fileBrowser.iconColor
                    opacity: parent.enabled ? 1 : 0.35
                }
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Forward")
                onClicked: pane.fileBrowser.goLocalForward()
            }

            ToolButton {
                implicitWidth: 30
                implicitHeight: 24
                contentItem: ArrowIcon {
                    anchors.centerIn: parent
                    direction: "up"
                    color: pane.fileBrowser.iconColor
                }
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Parent folder")
                onClicked: pane.fileBrowser.localParent()
            }

            ToolButton {
                implicitWidth: 30
                implicitHeight: 24
                contentItem: RefreshIcon {
                    anchors.centerIn: parent
                    color: pane.fileBrowser.iconColor
                    maskColor: pane.fileBrowser.panelColor
                }
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Refresh")
                onClicked: pane.fileBrowser.refreshLocal()
            }
        }

        TextField {
            Layout.fillWidth: true
            text: pane.fileBrowser.localPath
            selectByMouse: true
            color: pane.fileBrowser.primaryTextColor
            font.pixelSize: 11
            background: Rectangle {
                color: pane.fileBrowser.surfaceColor
                border.color: pane.fileBrowser.borderColor
                radius: 3
            }
            onAccepted: {
                pane.fileBrowser.enterLocal(text)
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 22
            color: pane.fileBrowser.surfaceColor

            Item {
                id: localHeaderInner
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8

                Repeater {
                    model: pane.fileBrowser.localColumnOrder
                    delegate: HeaderCell {
                        required property string modelData
                        required property int index
                        panelName: "local"
                        fileBrowser: pane.fileBrowser
                        colId: modelData
                        naturalIndex: index
                        parentWidth: localHeaderInner.width
                    }
                }
            }
        }

        ListView {
            id: localList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: pane.fileBrowser.localEntries
            reuseItems: true
            cacheBuffer: 600
            activeFocusOnTab: true
            highlightFollowsCurrentItem: true
            highlight: Rectangle {
                color: pane.fileBrowser.selectedColor
            }

            Keys.onPressed: function(event) {
                event.accepted = pane.fileBrowser.quickLocate("local", event.text, localList)
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.RightButton
                propagateComposedEvents: true
                z: 20
                onClicked: function(mouse) {
                    localList.forceActiveFocus()
                    if (localList.indexAt(mouse.x, mouse.y + localList.contentY) < 0) {
                        localBlankMenu.popup()
                    } else {
                        mouse.accepted = false
                    }
                }
            }

            OpenShellComponents.ThemedMenu {
                menuTheme: pane.fileBrowser.theme
                id: localBlankMenu
                OpenShellComponents.ThemedMenuItem {
                    theme: pane.fileBrowser.theme
                    text: qsTr("Refresh")
                    onTriggered: pane.fileBrowser.refreshLocal()
                }
                OpenShellComponents.ThemedMenuSeparator { theme: pane.fileBrowser.theme }
                OpenShellComponents.ThemedMenu {
                    menuTheme: pane.fileBrowser.theme
                    title: qsTr("Upload to Remote")
                    enabled: pane.fileBrowser.connectionId !== ""
                    OpenShellComponents.ThemedMenuItem {
                        theme: pane.fileBrowser.theme
                        text: qsTr("File")
                        onTriggered: pane.fileBrowser.chooseAndUploadFile()
                    }
                    OpenShellComponents.ThemedMenuItem {
                        theme: pane.fileBrowser.theme
                        text: qsTr("Folder")
                        onTriggered: pane.fileBrowser.chooseAndUploadFolder()
                    }
                }
            }

            delegate: Rectangle {
                id: localRow
                required property var modelData
                required property int index

                width: localList.width
                height: 26
                color: localList.currentIndex === index ? pane.fileBrowser.selectedColor
                      : mouseArea.containsMouse ? pane.fileBrowser.hoverColor : pane.fileBrowser.panelColor

                Drag.active: localDragHandler.active
                Drag.dragType: Drag.Automatic
                Drag.supportedActions: Qt.CopyAction
                Drag.mimeData: {
                    "text/uri-list": "file:///" + localRow.modelData.path.replace(/\\/g, "/")
                }
                Drag.hotSpot.x: 10
                Drag.hotSpot.y: height / 2

                DragHandler {
                    id: localDragHandler
                    target: null
                    acceptedButtons: Qt.LeftButton
                    onActiveChanged: {
                        pane.fileBrowser.draggedLocalPath = active ? localRow.modelData.path : ""
                    }
                }

                Item {
                    id: localRowInner
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8

                    Repeater {
                        model: pane.fileBrowser.localColumnOrder
                        delegate: DataCell {
                            required property string modelData
                            required property int index
                            panelName: "local"
                            fileBrowser: pane.fileBrowser
                            colId: modelData
                            naturalIndex: index
                            parentWidth: localRowInner.width
                            entry: localRow.modelData
                        }
                    }
                }

                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    hoverEnabled: true
                    onPressed: {
                        pane.fileBrowser.draggedLocalPath = localRow.modelData.path
                    }
                    onReleased: {
                        pane.fileBrowser.draggedLocalPath = ""
                    }
                    onCanceled: {
                        pane.fileBrowser.draggedLocalPath = ""
                    }
                    onClicked: function(mouse) {
                        localList.currentIndex = localRow.index
                        localList.forceActiveFocus()
                        if (mouse.button === Qt.RightButton) {
                            localItemMenu.popup()
                        }
                    }
                    onDoubleClicked: {
                        if (localRow.modelData.isDir) {
                            pane.fileBrowser.enterLocal(localRow.modelData.path)
                        }
                    }

                    OpenShellComponents.ThemedMenu {
                        menuTheme: pane.fileBrowser.theme
                        id: localItemMenu
                        OpenShellComponents.ThemedMenuItem {
                            theme: pane.fileBrowser.theme
                            text: qsTr("Upload to Remote")
                            enabled: pane.fileBrowser.connectionId !== ""
                            onTriggered: pane.fileBrowser.uploadLocalPath(localRow.modelData.path)
                        }
                    }
                }
            }

            ScrollBar.vertical: ScrollBar {}
        }
    }
}
