pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import ".." as OpenShellComponents

Window {
    id: remoteWindow

    required property var fileBrowser

    property bool pendingSystemMove: false

    onFrameSwapped: {
        if (pendingSystemMove) {
            pendingSystemMove = false
            startSystemMove()
        }
    }

    width: 760
    height: 520
    minimumWidth: 460
    minimumHeight: 300
    visible: false
    title: qsTr("Remote") + " - " + fileBrowser.remotePath
    color: fileBrowser.panelColor

    Rectangle {
        id: remoteWindowContent
        anchors.fill: parent
        color: remoteWindow.fileBrowser.panelColor

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: qsTr("Remote")
                    color: remoteWindow.fileBrowser.secondaryTextColor
                    font.bold: true
                    font.pixelSize: 12
                }
                ToolButton {
                    implicitWidth: 30
                    implicitHeight: 24
                    enabled: remoteWindow.fileBrowser.connectionId !== "" && remoteWindow.fileBrowser.remoteBackStack.length > 0
                    contentItem: ArrowIcon {
                        anchors.centerIn: parent
                        direction: "left"
                        color: remoteWindow.fileBrowser.iconColor
                        opacity: parent.enabled ? 1 : 0.35
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Back")
                    onClicked: remoteWindow.fileBrowser.goRemoteBack()
                }
                ToolButton {
                    implicitWidth: 30
                    implicitHeight: 24
                    enabled: remoteWindow.fileBrowser.connectionId !== "" && remoteWindow.fileBrowser.remoteForwardStack.length > 0
                    contentItem: ArrowIcon {
                        anchors.centerIn: parent
                        direction: "right"
                        color: remoteWindow.fileBrowser.iconColor
                        opacity: parent.enabled ? 1 : 0.35
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Forward")
                    onClicked: remoteWindow.fileBrowser.goRemoteForward()
                }
                ToolButton {
                    implicitWidth: 30
                    implicitHeight: 24
                    enabled: remoteWindow.fileBrowser.connectionId !== ""
                    contentItem: ArrowIcon {
                        anchors.centerIn: parent
                        direction: "up"
                        color: remoteWindow.fileBrowser.iconColor
                        opacity: parent.enabled ? 1 : 0.35
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Parent folder")
                    onClicked: remoteWindow.fileBrowser.remoteParent()
                }
                ToolButton {
                    implicitWidth: 30
                    implicitHeight: 24
                    enabled: remoteWindow.fileBrowser.connectionId !== ""
                    contentItem: RefreshIcon {
                        anchors.centerIn: parent
                        color: remoteWindow.fileBrowser.iconColor
                        maskColor: remoteWindow.fileBrowser.panelColor
                        opacity: parent.enabled ? 1 : 0.35
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Refresh")
                    onClicked: remoteWindow.fileBrowser.refreshRemote()
                }
            }

            TextField {
                Layout.fillWidth: true
                text: remoteWindow.fileBrowser.remotePath
                enabled: remoteWindow.fileBrowser.connectionId !== ""
                selectByMouse: true
                color: remoteWindow.fileBrowser.primaryTextColor
                font.pixelSize: 11
                background: Rectangle {
                    color: remoteWindow.fileBrowser.surfaceColor
                    border.color: remoteWindow.fileBrowser.borderColor
                    radius: 3
                }
                onAccepted: {
                    remoteWindow.fileBrowser.enterRemote(text)
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 22
                color: remoteWindow.fileBrowser.surfaceColor
                Item {
                    id: detachedRemoteHeaderInner
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    Repeater {
                        model: remoteWindow.fileBrowser.remoteColumnOrder
                        delegate: HeaderCell {
                            required property string modelData
                            required property int index
                            panelName: "remote"
                            fileBrowser: remoteWindow.fileBrowser
                            colId: modelData
                            naturalIndex: index
                            parentWidth: detachedRemoteHeaderInner.width
                        }
                    }
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: remoteWindow.fileBrowser.remoteListingLoading || (remoteWindow.fileBrowser.remoteError.length > 0 && remoteWindow.fileBrowser.remoteEntries.length === 0) ? 1 : 0

                ListView {
                    id: detachedRemoteList
                    clip: true
                    model: remoteWindow.fileBrowser.remoteEntries
                    reuseItems: true
                    cacheBuffer: 900

                    delegate: Rectangle {
                        id: detachedRemoteRow
                        required property var modelData
                        required property int index
                        width: detachedRemoteList.width
                        height: 26
                        color: detachedRemoteMouse.containsMouse ? remoteWindow.fileBrowser.hoverColor
                                                                 : remoteWindow.fileBrowser.panelColor

                        Item {
                            id: detachedRemoteRowInner
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            Repeater {
                                model: remoteWindow.fileBrowser.remoteColumnOrder
                                delegate: DataCell {
                                    required property string modelData
                                    required property int index
                                    panelName: "remote"
                                    fileBrowser: remoteWindow.fileBrowser
                                    colId: modelData
                                    naturalIndex: index
                                    parentWidth: detachedRemoteRowInner.width
                                    entry: detachedRemoteRow.modelData
                                }
                            }
                        }

                        MouseArea {
                            id: detachedRemoteMouse
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            hoverEnabled: true
                            onDoubleClicked: {
                                if (detachedRemoteRow.modelData.isDir) {
                                    remoteWindow.fileBrowser.enterRemote(detachedRemoteRow.modelData.path)
                                } else {
                                    remoteWindow.fileBrowser.openRemotePath(detachedRemoteRow.modelData.path)
                                }
                            }
                            onClicked: function(mouse) {
                                if (mouse.button === Qt.RightButton) {
                                    remoteWindow.fileBrowser.remoteMenuEntry = detachedRemoteRow.modelData
                                    detachedRemoteMenu.popup()
                                }
                            }
                        }
                    }

                    OpenShellComponents.ThemedMenu {
                        id: detachedRemoteMenu
                        menuTheme: remoteWindow.fileBrowser.theme
                        readonly property var entry: remoteWindow.fileBrowser.remoteMenuEntry || ({})
                        readonly property bool hasEntry: entry && entry.path
                        readonly property bool isDir: hasEntry && entry.isDir
                        OpenShellComponents.ThemedMenuItem {
                            theme: remoteWindow.fileBrowser.theme
                            text: qsTr("Download")
                            enabled: detachedRemoteMenu.hasEntry
                            onTriggered: remoteWindow.fileBrowser.downloadRemotePath(detachedRemoteMenu.entry.path)
                        }
                        OpenShellComponents.ThemedMenu {
                            menuTheme: remoteWindow.fileBrowser.theme
                            title: qsTr("Upload...")
                            enabled: detachedRemoteMenu.hasEntry && remoteWindow.fileBrowser.connectionId !== ""
                            OpenShellComponents.ThemedMenuItem {
                                theme: remoteWindow.fileBrowser.theme
                                text: qsTr("File")
                                onTriggered: remoteWindow.fileBrowser.chooseAndUploadFileTo(remoteWindow.fileBrowser.uploadTargetForRemoteMenuEntry())
                            }
                            OpenShellComponents.ThemedMenuItem {
                                theme: remoteWindow.fileBrowser.theme
                                text: qsTr("Folder")
                                onTriggered: remoteWindow.fileBrowser.chooseAndUploadFolderTo(remoteWindow.fileBrowser.uploadTargetForRemoteMenuEntry())
                            }
                        }
                        OpenShellComponents.ThemedMenuSeparator { theme: remoteWindow.fileBrowser.theme }
                        OpenShellComponents.ThemedMenuItem {
                            theme: remoteWindow.fileBrowser.theme
                            text: qsTr("Rename")
                            enabled: detachedRemoteMenu.hasEntry
                            onTriggered: remoteWindow.fileBrowser.openNameDialog("rename",
                                                                                detachedRemoteMenu.entry.path,
                                                                                detachedRemoteMenu.entry.name)
                        }
                        OpenShellComponents.ThemedMenuItem {
                            theme: remoteWindow.fileBrowser.theme
                            text: qsTr("Delete")
                            enabled: detachedRemoteMenu.hasEntry
                            onTriggered: remoteWindow.fileBrowser.deleteRemotePath(detachedRemoteMenu.entry.path, false)
                        }
                    }

                    ScrollBar.vertical: ScrollBar {}
                }

                Label {
                    text: remoteWindow.fileBrowser.remoteListingLoading ? qsTr("Loading...") : remoteWindow.fileBrowser.remoteError
                    color: remoteWindow.fileBrowser.mutedTextColor
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    onClosing: function(close) {
        close.accepted = true
        destroy()
    }
    Component.onDestruction: {
        remoteWindow.fileBrowser.remoteDetachedWindow = null
        remoteWindow.fileBrowser.updateDetachedPaneState()
    }
}
