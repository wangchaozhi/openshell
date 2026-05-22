pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".." as OpenShellComponents

// 文件浏览器的“远程”窗格。从 FileBrowser.qml 内联块拆出，逻辑全部走
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
        active: pane.fileBrowser.remoteDetaching
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        RowLayout {
            Layout.fillWidth: true

            Label {
                id: remoteTitleLabel
                text: qsTr("Remote")
                color: pane.fileBrowser.secondaryTextColor
                font.bold: true
                font.pixelSize: 12

                DetachGesture {
                    onArmedChanged: pane.fileBrowser.remoteDetaching = armed
                    onDetachTriggered: pane.fileBrowser.detachRemotePane(remoteTitleLabel)
                }
            }

            ToolButton {
                id: remoteDetachButton
                implicitWidth: 30
                implicitHeight: 24
                enabled: pane.fileBrowser.connectionId !== ""
                contentItem: DetachIcon {
                    anchors.centerIn: parent
                    color: pane.fileBrowser.iconColor
                    accentColor: pane.fileBrowser.iconAccentColor
                    opacity: parent.enabled ? 1 : 0.35
                }
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Detach Window")

                DetachGesture {
                    onArmedChanged: pane.fileBrowser.remoteDetaching = armed
                    onDetachTriggered: pane.fileBrowser.detachRemotePane(remoteDetachButton)
                }
            }

            ToolButton {
                implicitWidth: 30
                implicitHeight: 24
                enabled: pane.fileBrowser.connectionId !== "" && pane.fileBrowser.remoteBackStack.length > 0
                contentItem: ArrowIcon {
                    anchors.centerIn: parent
                    direction: "left"
                    color: pane.fileBrowser.iconColor
                    opacity: parent.enabled ? 1 : 0.35
                }
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Back")
                onClicked: pane.fileBrowser.goRemoteBack()
            }

            ToolButton {
                implicitWidth: 30
                implicitHeight: 24
                enabled: pane.fileBrowser.connectionId !== "" && pane.fileBrowser.remoteForwardStack.length > 0
                contentItem: ArrowIcon {
                    anchors.centerIn: parent
                    direction: "right"
                    color: pane.fileBrowser.iconColor
                    opacity: parent.enabled ? 1 : 0.35
                }
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Forward")
                onClicked: pane.fileBrowser.goRemoteForward()
            }

            ToolButton {
                implicitWidth: 30
                implicitHeight: 24
                enabled: pane.fileBrowser.connectionId !== ""
                contentItem: ArrowIcon {
                    anchors.centerIn: parent
                    direction: "up"
                    color: pane.fileBrowser.iconColor
                    opacity: parent.enabled ? 1 : 0.35
                }
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Parent folder")
                onClicked: pane.fileBrowser.remoteParent()
            }

            ToolButton {
                implicitWidth: 30
                implicitHeight: 24
                enabled: pane.fileBrowser.connectionId !== ""
                contentItem: RefreshIcon {
                    anchors.centerIn: parent
                    color: pane.fileBrowser.iconColor
                    maskColor: pane.fileBrowser.panelColor
                    opacity: parent.enabled ? 1 : 0.35
                }
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Refresh")
                onClicked: pane.fileBrowser.refreshRemote()
            }

            ToolButton {
                id: syncButton
                implicitWidth: 30
                implicitHeight: 24
                enabled: pane.fileBrowser.connectionId !== ""
                checkable: true
                checked: pane.fileBrowser.syncRemoteWithTerminal
                onClicked: pane.fileBrowser.syncRemoteWithTerminal = checked
                background: Rectangle {
                    color: pane.fileBrowser.sessionProblem ? "#2a1014"
                          : !pane.fileBrowser.sessionConnected ? "#241b0a"
                          : syncButton.checked ? (pane.fileBrowser.classic ? "#e0f2fe" : "#0f2742") : pane.fileBrowser.hoverColor
                    border.color: pane.fileBrowser.sessionProblem ? "#ef4444"
                                  : !pane.fileBrowser.sessionConnected ? "#f59e0b"
                                  : syncButton.checked ? "#38bdf8" : pane.fileBrowser.mutedBorderColor
                    radius: 3
                }
                contentItem: SyncIcon {
                    anchors.centerIn: parent
                    active: pane.fileBrowser.syncRemoteWithTerminal
                    connected: pane.fileBrowser.sessionConnected
                    problem: pane.fileBrowser.sessionProblem
                    maskColor: pane.fileBrowser.panelColor
                    opacity: parent.enabled ? 1 : 0.35
                }
                ToolTip.visible: hovered
                ToolTip.text: checked ? qsTr("Sync with terminal folder: on")
                                      : qsTr("Sync with terminal folder: off")
            }

            ToolButton {
                implicitWidth: 30
                implicitHeight: 24
                contentItem: SettingsIcon {
                    anchors.centerIn: parent
                    color: pane.fileBrowser.iconColor
                    centerColor: pane.fileBrowser.panelColor
                }
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Remote file open settings")
                onClicked: pane.fileBrowser.openRemoteOpenSettings()
            }

            Item {
                Layout.fillWidth: true
            }

            ToolButton {
                implicitWidth: 46
                implicitHeight: 24
                text: pane.fileBrowser.transferTasks.length > 0 ? String(pane.fileBrowser.transferTasks.length) : ""
                enabled: pane.fileBrowser.transferTasks.length > 0
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Transfer tasks")
                onClicked: pane.fileBrowser.transferPanelOpen = !pane.fileBrowser.transferPanelOpen
                background: Rectangle {
                    color: parent.enabled ? pane.fileBrowser.hoverColor : pane.fileBrowser.panelColor
                    border.color: pane.fileBrowser.transferPanelOpen ? "#38bdf8" : pane.fileBrowser.mutedBorderColor
                    radius: 3
                }
                contentItem: Row {
                    spacing: 4
                    anchors.centerIn: parent
                    Rectangle {
                        width: 12
                        height: 10
                        radius: 2
                        color: pane.fileBrowser.transferTasks.length > 0 ? pane.fileBrowser.iconAccentColor : pane.fileBrowser.iconColor
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Label {
                        text: pane.fileBrowser.transferTasks.length > 0 ? String(pane.fileBrowser.transferTasks.length) : ""
                        color: pane.fileBrowser.primaryTextColor
                        font.pixelSize: 11
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }

        TextField {
            Layout.fillWidth: true
            text: pane.fileBrowser.remotePath
            enabled: pane.fileBrowser.connectionId !== ""
            selectByMouse: true
            color: pane.fileBrowser.primaryTextColor
            font.pixelSize: 11
            background: Rectangle {
                color: pane.fileBrowser.surfaceColor
                border.color: pane.fileBrowser.borderColor
                radius: 3
            }
            onAccepted: {
                pane.fileBrowser.enterRemote(text)
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 22
            color: pane.fileBrowser.surfaceColor

            Item {
                id: remoteHeaderInner
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8

                Repeater {
                    model: pane.fileBrowser.remoteColumnOrder
                    delegate: HeaderCell {
                        required property string modelData
                        required property int index
                        panelName: "remote"
                        fileBrowser: pane.fileBrowser
                        colId: modelData
                        naturalIndex: index
                        parentWidth: remoteHeaderInner.width
                    }
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: pane.fileBrowser.remoteListingLoading || (pane.fileBrowser.remoteError.length > 0 && pane.fileBrowser.remoteEntries.length === 0) ? 1 : 0

            ListView {
                id: remoteList
                clip: true
                model: pane.fileBrowser.remoteEntries
                reuseItems: true
                cacheBuffer: 900
                activeFocusOnTab: true
                highlightFollowsCurrentItem: true
                highlight: Rectangle {
                    color: pane.fileBrowser.selectedColor
                }

                Keys.onPressed: function(event) {
                    event.accepted = pane.fileBrowser.quickLocate("remote", event.text, remoteList)
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    propagateComposedEvents: true
                    z: 20
                    onClicked: function(mouse) {
                        remoteList.forceActiveFocus()
                        if (remoteList.indexAt(mouse.x, mouse.y + remoteList.contentY) < 0) {
                            remoteBlankMenu.popup()
                        } else {
                            mouse.accepted = false
                        }
                    }
                }

                OpenShellComponents.ThemedMenu {
                    menuTheme: pane.fileBrowser.theme
                    id: remoteBlankMenu
                    OpenShellComponents.ThemedMenuItem {
                        theme: pane.fileBrowser.theme
                        text: qsTr("Refresh")
                        enabled: pane.fileBrowser.connectionId !== ""
                        onTriggered: pane.fileBrowser.refreshRemote()
                    }
                    OpenShellComponents.ThemedMenuSeparator { theme: pane.fileBrowser.theme }
                    OpenShellComponents.ThemedMenu {
                        menuTheme: pane.fileBrowser.theme
                        title: qsTr("Upload...")
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
                    OpenShellComponents.ThemedMenuSeparator { theme: pane.fileBrowser.theme }
                    OpenShellComponents.ThemedMenu {
                        menuTheme: pane.fileBrowser.theme
                        title: qsTr("New")
                        enabled: pane.fileBrowser.connectionId !== ""
                        OpenShellComponents.ThemedMenuItem {
                            theme: pane.fileBrowser.theme
                            text: qsTr("File")
                            onTriggered: pane.fileBrowser.openNameDialog("newFile", "", "new-file")
                        }
                        OpenShellComponents.ThemedMenuItem {
                            theme: pane.fileBrowser.theme
                            text: qsTr("Folder")
                            onTriggered: pane.fileBrowser.openNameDialog("newDir", "", "new-folder")
                        }
                    }
                }

                OpenShellComponents.ThemedMenu {
                    menuTheme: pane.fileBrowser.theme
                    id: remoteItemMenu
                    readonly property var entry: pane.fileBrowser.remoteMenuEntry || ({})
                    readonly property bool hasEntry: !!(entry && entry.path)
                    readonly property bool isDir: hasEntry && !!entry.isDir

                    OpenShellComponents.ThemedMenuItem {
                        theme: pane.fileBrowser.theme
                        text: qsTr("Refresh")
                        onTriggered: pane.fileBrowser.refreshRemote()
                    }
                    OpenShellComponents.ThemedMenuSeparator { theme: pane.fileBrowser.theme }
                    OpenShellComponents.ThemedMenuItem {
                        theme: pane.fileBrowser.theme
                        text: qsTr("Open")
                        enabled: remoteItemMenu.hasEntry && !remoteItemMenu.isDir
                        onTriggered: pane.fileBrowser.openRemotePath(remoteItemMenu.entry.path)
                    }
                    OpenShellComponents.ThemedMenu {
                        menuTheme: pane.fileBrowser.theme
                        title: qsTr("Open With")
                        enabled: remoteItemMenu.hasEntry && !remoteItemMenu.isDir
                        OpenShellComponents.ThemedMenuItem {
                            theme: pane.fileBrowser.theme
                            text: qsTr("System default app")
                            onTriggered: {
                                appController.remoteFileOpenMode = "system"
                                pane.fileBrowser.openRemotePath(remoteItemMenu.entry.path)
                            }
                        }
                        OpenShellComponents.ThemedMenuItem {
                            theme: pane.fileBrowser.theme
                            text: qsTr("Specified text editor")
                            onTriggered: {
                                appController.remoteFileOpenMode = "custom"
                                pane.fileBrowser.openRemotePath(remoteItemMenu.entry.path)
                            }
                        }
                        OpenShellComponents.ThemedMenuItem {
                            theme: pane.fileBrowser.theme
                            text: qsTr("Built-in editor")
                            onTriggered: {
                                appController.remoteFileOpenMode = "internal"
                                pane.fileBrowser.openRemotePath(remoteItemMenu.entry.path)
                            }
                        }
                    }
                    OpenShellComponents.ThemedMenu {
                        menuTheme: pane.fileBrowser.theme
                        title: qsTr("Select Text Editor")
                        OpenShellComponents.ThemedMenuItem {
                            theme: pane.fileBrowser.theme
                            text: appController.externalTextEditorPath && appController.externalTextEditorPath.length > 0
                                  ? appController.externalTextEditorPath
                                  : qsTr("Browse...")
                            onTriggered: pane.fileBrowser.chooseExternalEditor()
                        }
                        OpenShellComponents.ThemedMenuItem {
                            theme: pane.fileBrowser.theme
                            text: qsTr("Open settings")
                            onTriggered: pane.fileBrowser.openRemoteOpenSettings()
                        }
                    }
                    OpenShellComponents.ThemedMenuSeparator { theme: pane.fileBrowser.theme }
                    OpenShellComponents.ThemedMenuItem {
                        theme: pane.fileBrowser.theme
                        text: qsTr("Copy Path")
                        enabled: remoteItemMenu.hasEntry
                        onTriggered: appController.copyTextToClipboard(remoteItemMenu.entry.path)
                    }
                    OpenShellComponents.ThemedMenuSeparator { theme: pane.fileBrowser.theme }
                    OpenShellComponents.ThemedMenuItem {
                        theme: pane.fileBrowser.theme
                        text: qsTr("Download")
                        enabled: remoteItemMenu.hasEntry
                        onTriggered: pane.fileBrowser.downloadRemotePath(remoteItemMenu.entry.path)
                    }
                    OpenShellComponents.ThemedMenu {
                        menuTheme: pane.fileBrowser.theme
                        title: qsTr("Upload...")
                        enabled: remoteItemMenu.hasEntry && pane.fileBrowser.connectionId !== ""
                        OpenShellComponents.ThemedMenuItem {
                            theme: pane.fileBrowser.theme
                            text: qsTr("File")
                            onTriggered: pane.fileBrowser.chooseAndUploadFileTo(pane.fileBrowser.uploadTargetForRemoteMenuEntry())
                        }
                        OpenShellComponents.ThemedMenuItem {
                            theme: pane.fileBrowser.theme
                            text: qsTr("Folder")
                            onTriggered: pane.fileBrowser.chooseAndUploadFolderTo(pane.fileBrowser.uploadTargetForRemoteMenuEntry())
                        }
                    }
                    OpenShellComponents.ThemedMenu {
                        menuTheme: pane.fileBrowser.theme
                        title: qsTr("Transfer Package")
                        enabled: false
                        OpenShellComponents.ThemedMenuItem { theme: pane.fileBrowser.theme; text: qsTr("Coming soon") }
                    }
                    OpenShellComponents.ThemedMenuSeparator { theme: pane.fileBrowser.theme }
                    OpenShellComponents.ThemedMenu {
                        menuTheme: pane.fileBrowser.theme
                        title: qsTr("New")
                        enabled: remoteItemMenu.hasEntry
                        OpenShellComponents.ThemedMenuItem {
                            theme: pane.fileBrowser.theme
                            text: qsTr("File")
                            onTriggered: pane.fileBrowser.openNameDialog("newFile", "", "new-file")
                        }
                        OpenShellComponents.ThemedMenuItem {
                            theme: pane.fileBrowser.theme
                            text: qsTr("Folder")
                            onTriggered: pane.fileBrowser.openNameDialog("newDir", "", "new-folder")
                        }
                    }
                    OpenShellComponents.ThemedMenuSeparator { theme: pane.fileBrowser.theme }
                    OpenShellComponents.ThemedMenuItem {
                        theme: pane.fileBrowser.theme
                        text: qsTr("Rename")
                        enabled: remoteItemMenu.hasEntry
                        onTriggered: pane.fileBrowser.openNameDialog("rename",
                                                                     remoteItemMenu.entry.path,
                                                                     remoteItemMenu.entry.name)
                    }
                    OpenShellComponents.ThemedMenuItem {
                        theme: pane.fileBrowser.theme
                        text: qsTr("Delete")
                        enabled: remoteItemMenu.hasEntry
                        onTriggered: pane.fileBrowser.deleteRemotePath(remoteItemMenu.entry.path, false)
                    }
                    OpenShellComponents.ThemedMenuItem {
                        theme: pane.fileBrowser.theme
                        text: qsTr("Quick Delete (rm)")
                        enabled: remoteItemMenu.hasEntry
                        onTriggered: pane.fileBrowser.deleteRemotePath(remoteItemMenu.entry.path, true)
                    }
                    OpenShellComponents.ThemedMenuSeparator { theme: pane.fileBrowser.theme }
                    OpenShellComponents.ThemedMenuItem {
                        theme: pane.fileBrowser.theme
                        text: qsTr("Permissions (%1)").arg(remoteItemMenu.entry.permissions || qsTr("?"))
                        enabled: remoteItemMenu.hasEntry
                        onTriggered: pane.fileBrowser.openChmodDialog(remoteItemMenu.entry.path,
                                                                      remoteItemMenu.entry.permissions || "",
                                                                      remoteItemMenu.isDir)
                    }
                }

                delegate: Rectangle {
                    id: remoteRow
                    required property var modelData
                    required property int index

                    width: remoteList.width
                    height: 26
                    color: remoteRowDropArea.containsDrag ? pane.fileBrowser.dropHoverColor
                          : remoteList.currentIndex === index ? pane.fileBrowser.selectedColor
                          : remoteMouseArea.containsMouse ? pane.fileBrowser.hoverColor : pane.fileBrowser.panelColor

                    Item {
                        id: remoteRowInner
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8

                        Repeater {
                            model: pane.fileBrowser.remoteColumnOrder
                            delegate: DataCell {
                                required property string modelData
                                required property int index
                                panelName: "remote"
                                fileBrowser: pane.fileBrowser
                                colId: modelData
                                naturalIndex: index
                                parentWidth: remoteRowInner.width
                                entry: remoteRow.modelData
                            }
                        }
                    }

                    MouseArea {
                        id: remoteMouseArea
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        hoverEnabled: true
                        onClicked: function(mouse) {
                            remoteList.currentIndex = remoteRow.index
                            remoteList.forceActiveFocus()
                            if (mouse.button === Qt.RightButton) {
                                pane.fileBrowser.remoteMenuEntry = remoteRow.modelData
                                remoteItemMenu.popup()
                            }
                        }
                        onDoubleClicked: {
                            if (remoteRow.modelData.isDir) {
                                pane.fileBrowser.enterRemote(remoteRow.modelData.path)
                            } else {
                                pane.fileBrowser.openRemotePath(remoteRow.modelData.path)
                            }
                        }
                    }

                    DropArea {
                        id: remoteRowDropArea
                        anchors.fill: parent
                        enabled: pane.fileBrowser.connectionId !== "" && remoteRow.modelData.isDir
                        keys: [ "text/uri-list" ]
                        onEntered: function(drag) {
                            pane.fileBrowser.remoteDropActive = true
                            pane.fileBrowser.dropRemoteTargetPath = remoteRow.modelData.path
                            drag.accepted = true
                        }
                        onExited: {
                            pane.fileBrowser.dropRemoteTargetPath = ""
                        }
                        onDropped: function(drop) {
                            pane.fileBrowser.handleRemoteDrop(drop, remoteRow.modelData.path)
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar {}
            }

            Label {
                text: pane.fileBrowser.remoteListingLoading ? qsTr("Loading...") : pane.fileBrowser.remoteError
                color: pane.fileBrowser.mutedTextColor
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.WordWrap
            }
        }
    }

    TransferPanel {
        fileBrowser: pane.fileBrowser
    }

    DropArea {
        id: remotePanelDropArea
        anchors.fill: parent
        anchors.topMargin: 58
        enabled: pane.fileBrowser.connectionId !== ""
        keys: [ "text/uri-list" ]
        onEntered: function(drag) {
            pane.fileBrowser.remoteDropActive = true
            pane.fileBrowser.dropRemoteTargetPath = pane.fileBrowser.remotePath
            drag.accepted = true
        }
        onExited: {
            pane.fileBrowser.remoteDropActive = false
            pane.fileBrowser.dropRemoteTargetPath = ""
        }
        onDropped: function(drop) {
            pane.fileBrowser.handleRemoteDrop(drop, pane.fileBrowser.remotePath)
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 8
        visible: pane.fileBrowser.remoteDropActive
        color: pane.fileBrowser.theme.focus
        opacity: 0.12
        border.color: pane.fileBrowser.theme.focus
        border.width: 2
        radius: 4
        z: 20

        Label {
            anchors.centerIn: parent
            text: pane.fileBrowser.dropRemoteTargetPath.length > 0
                  ? qsTr("Upload to %1").arg(pane.fileBrowser.dropRemoteTargetPath)
                  : qsTr("Upload to Remote")
            color: pane.fileBrowser.activeTextColor
            font.pixelSize: 13
            font.bold: true
            elide: Text.ElideMiddle
            width: parent.width - 40
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
