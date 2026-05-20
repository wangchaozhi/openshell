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
    readonly property bool classic: uiTheme === "classic"

    signal sessionActivated(string id)
    signal sessionClosed(string id)
    signal sessionDisconnected(string id)
    signal sessionReconnectRequested(string id, string connectionId)
    signal connectionManagerActivated()
    signal systemInfoActivated()
    signal systemInfoClosed()
    color: classic ? "#f8fafc" : "#020617"

    component CloseIcon: Item {
        implicitWidth: 12
        implicitHeight: 12
        Rectangle {
            anchors.centerIn: parent
            width: 13
            height: 2
            radius: 1
            rotation: 45
            color: root.classic ? "#475569" : "#cbd5e1"
        }
        Rectangle {
            anchors.centerIn: parent
            width: 13
            height: 2
            radius: 1
            rotation: -45
            color: root.classic ? "#475569" : "#cbd5e1"
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
                       ? (root.classic ? "#e0f2fe" : "#1e293b")
                       : (root.classic ? "#ffffff" : "#0f172a")
                border.color: root.activeView === "connections"
                              ? "#38bdf8"
                              : (root.classic ? "#cbd5e1" : "#1e293b")
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
                        border.color: root.classic ? "#0284c7" : "#38bdf8"
                        border.width: 2
                    }
                    Rectangle {
                        x: 8
                        y: 13
                        width: 4
                        height: 3
                        radius: 1
                        color: root.classic ? "#0284c7" : "#38bdf8"
                    }
                    Rectangle {
                        x: 5
                        y: 16
                        width: 10
                        height: 2
                        radius: 1
                        color: root.classic ? "#0284c7" : "#38bdf8"
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

                color: isActive && root.activeView === "terminal"
                       ? (root.classic ? "#e0f2fe" : "#1e293b")
                       : (root.classic ? "#ffffff" : "#0f172a")
                border.color: isActive && root.activeView === "terminal"
                              ? "#38bdf8"
                              : (root.classic ? "#cbd5e1" : "#1e293b")
                border.width: 1

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    cursorShape: Qt.PointingHandCursor
                    onClicked: function(mouse) {
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

                Menu {
                    id: tabMenu
                    MenuItem {
                        text: tab.isConnected ? qsTr("Disconnect") : qsTr("Disconnected")
                        enabled: tab.isConnected
                        onTriggered: root.sessionDisconnected(modelData.id)
                    }
                    MenuItem {
                        text: qsTr("Reconnect")
                        enabled: modelData.connectionId && modelData.connectionId.length > 0
                        onTriggered: root.sessionReconnectRequested(modelData.id, modelData.connectionId)
                    }
                    MenuSeparator {}
                    MenuItem {
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
                               ? (root.classic ? "#0f172a" : "#f1f5f9")
                               : (root.classic ? "#334155" : "#cbd5f5")
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
                   ? (root.classic ? "#e0f2fe" : "#1e293b")
                   : (root.classic ? "#ffffff" : "#0f172a")
            border.color: root.activeView === "system"
                          ? "#38bdf8"
                          : (root.classic ? "#cbd5e1" : "#1e293b")
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
                           ? (root.classic ? "#0f172a" : "#f1f5f9")
                           : (root.classic ? "#334155" : "#cbd5f5")
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
