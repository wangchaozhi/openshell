import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var sessions: []
    property string activeSessionId: ""
    property string activeView: "terminal"
    property string uiTheme: "dark"
    property bool systemInfoTabVisible: false
    property alias themePalette: theme
    readonly property bool classic: theme.classic

    signal sessionActivated(string id)
    signal sessionClosed(string id)
    signal sessionDisconnected(string id)
    signal sessionReconnectRequested(string id, string connectionId)
    signal sessionDetached(string id)
    signal connectionManagerActivated()
    signal systemInfoActivated()
    signal systemInfoClosed()
    ThemePalette {
        id: theme
        mode: root.uiTheme
    }

    color: theme.surface

    component CloseIcon: Item {
        implicitWidth: 12
        implicitHeight: 12
        Rectangle {
            anchors.centerIn: parent
            width: 13
            height: 2
            radius: 1
            rotation: 45
            color: theme.icon
        }
        Rectangle {
            anchors.centerIn: parent
            width: 13
            height: 2
            radius: 1
            rotation: -45
            color: theme.icon
        }
    }

    Row {
        anchors.fill: parent
        anchors.leftMargin: 4
        spacing: 2

        ToolButton {
            width: 38
            height: parent.height - 4
            anchors.verticalCenter: parent.verticalCenter
            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Connection Manager")
            background: Rectangle {
                radius: 4
                color: root.activeView === "connections"
                       ? theme.selected
                       : theme.panel
                border.color: root.activeView === "connections"
                              ? theme.focus
                              : theme.border
            }
            contentItem: Item {
                Item {
                    width: 20
                    height: 18
                    anchors.centerIn: parent

                    Rectangle {
                        x: 2
                        y: 2
                        width: 16
                        height: 11
                        radius: 2
                        color: "transparent"
                        border.color: theme.iconAccent
                        border.width: 2
                    }
                    Rectangle {
                        x: 8
                        y: 13
                        width: 4
                        height: 3
                        radius: 1
                        color: theme.iconAccent
                    }
                    Rectangle {
                        x: 5
                        y: 16
                        width: 10
                        height: 2
                        radius: 1
                        color: theme.iconAccent
                    }
                }
            }
            onClicked: root.connectionManagerActivated()
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
                readonly property bool isConnected: modelData.status === "connected"
                property real pressX: 0
                property real pressY: 0
                property bool detachedByDrag: false
                property bool detachHoldReady: false
                property bool suppressClickAfterHold: false

                color: isActive && root.activeView === "terminal"
                       ? theme.selected
                       : theme.panel
                border.color: isActive && root.activeView === "terminal"
                              ? theme.focus
                              : theme.border
                border.width: 1

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    cursorShape: Qt.PointingHandCursor
                    onPressed: function(mouse) {
                        tab.pressX = mouse.x
                        tab.pressY = mouse.y
                        tab.detachedByDrag = false
                        tab.detachHoldReady = false
                        tab.suppressClickAfterHold = false
                    }
                    onPressAndHold: {
                        tab.detachHoldReady = true
                        tab.suppressClickAfterHold = true
                    }
                    onPositionChanged: function(mouse) {
                        if ((mouse.buttons & Qt.LeftButton) === 0
                                || tab.detachedByDrag
                                || !tab.detachHoldReady) {
                            return
                        }
                        const dx = mouse.x - tab.pressX
                        const dy = mouse.y - tab.pressY
                        if (Math.sqrt(dx * dx + dy * dy) > 8) {
                            tab.detachedByDrag = true
                            root.sessionDetached(modelData.id)
                        }
                    }
                    onReleased: {
                        tab.detachHoldReady = false
                    }
                    onCanceled: {
                        tab.detachHoldReady = false
                    }
                    onClicked: function(mouse) {
                        if (tab.detachedByDrag || tab.suppressClickAfterHold) {
                            return
                        }
                        if (mouse.button === Qt.RightButton) {
                            tabMenu.popup()
                        } else {
                            root.sessionActivated(modelData.id)
                        }
                    }
                    onDoubleClicked: {
                        root.sessionReconnectRequested(modelData.id, modelData.connectionId || "")
                    }
                }

                ThemedMenu {
                    id: tabMenu
                    menuTheme: root.themePalette
                    ThemedMenuItem {
                        theme: root.themePalette
                        text: tab.isConnected ? qsTr("Disconnect") : qsTr("Disconnected")
                        enabled: tab.isConnected
                        onTriggered: root.sessionDisconnected(modelData.id)
                    }
                    ThemedMenuItem {
                        theme: root.themePalette
                        text: qsTr("Reconnect")
                        enabled: modelData.connectionId && modelData.connectionId.length > 0
                        onTriggered: root.sessionReconnectRequested(modelData.id, modelData.connectionId)
                    }
                    ThemedMenuSeparator { theme: root.themePalette }
                    ThemedMenuItem {
                        theme: root.themePalette
                        text: qsTr("Detach Window")
                        onTriggered: root.sessionDetached(modelData.id)
                    }
                    ThemedMenuItem {
                        theme: root.themePalette
                        text: qsTr("Close session")
                        onTriggered: root.sessionClosed(modelData.id)
                    }
                }

                RowLayout {
                    id: tabRow
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 6
                    spacing: 6

                    Label {
                        text: modelData.title || qsTr("session")
                        color: tab.isActive && root.activeView === "terminal"
                               ? theme.textPrimary
                               : theme.textSecondary
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

        Rectangle {
            id: sysTab
            visible: root.systemInfoTabVisible
            width: sysTabRow.implicitWidth + 16
            height: parent.height - 4
            anchors.verticalCenter: parent.verticalCenter
            radius: 4
            color: root.activeView === "system"
                   ? theme.selected
                   : theme.panel
            border.color: root.activeView === "system"
                          ? theme.focus
                          : theme.border
            border.width: 1

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                cursorShape: Qt.PointingHandCursor
                onClicked: root.systemInfoActivated()
            }

            RowLayout {
                id: sysTabRow
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 6
                spacing: 6

                Label {
                    text: qsTr("System Info")
                    color: root.activeView === "system"
                           ? theme.textPrimary
                           : theme.textSecondary
                    font.pixelSize: 12
                }

                ToolButton {
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    contentItem: CloseIcon {
                        anchors.centerIn: parent
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Close")
                    onClicked: root.systemInfoClosed()
                }
            }
        }
    }
}
