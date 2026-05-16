import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: window

    width: 1180
    height: 720
    minimumWidth: 880
    minimumHeight: 540
    visible: true
    title: qsTr("OpenShell")
    color: "#0f172a"

    property var connections: appController.connectionProfiles()
    property var activeSessions: appController.sessions()
    property string selectedConnectionId: appController.currentConnectionId
    property string statusMessage: ""

    function refreshConnections() {
        connections = appController.reloadConnectionProfiles()
    }

    function refreshSessions() {
        activeSessions = appController.sessions()
    }

    Component.onCompleted: {
        const saved = appController.mainWindowGeometry()
        if (saved && saved.width > 0 && saved.height > 0) {
            x = saved.x
            y = saved.y
            width = Math.max(minimumWidth, saved.width)
            height = Math.max(minimumHeight, saved.height)
        }
    }

    Timer {
        id: geometrySaveTimer
        interval: 600
        onTriggered: appController.saveMainWindowGeometry(window.x, window.y, window.width, window.height)
    }

    onXChanged: if (visible) geometrySaveTimer.restart()
    onYChanged: if (visible) geometrySaveTimer.restart()
    onWidthChanged: if (visible) geometrySaveTimer.restart()
    onHeightChanged: if (visible) geometrySaveTimer.restart()

    Connections {
        target: appController
        function onSessionsChanged() { window.refreshSessions() }
        function onLanguageChanged() {
            window.title = qsTr("OpenShell")
            window.refreshConnections()
        }
        function onShowRequested() {
            window.show()
            window.raise()
            window.requestActivate()
        }
        function onHideRequested() {
            if (appController.minimizeToTray) {
                window.hide()
            }
        }
    }

    onClosing: function(close) {
        if (appController.minimizeToTray) {
            close.accepted = false
            window.hide()
        }
    }

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        Sidebar {
            SplitView.preferredWidth: 280
            SplitView.minimumWidth: 220

            connections: window.connections
            selectedConnectionId: window.selectedConnectionId

            onConnectionPicked: (id) => window.selectedConnectionId = id
            onConnectionDoublePicked: (id) => appController.openSession(id)
            onNewConnectionRequested: connectionEditor.openForNew()
            onEditConnectionRequested: (id) => connectionEditor.openForEdit(id)
            onDeleteConnectionRequested: (id) => {
                if (appController.deleteConnection(id)) {
                    window.statusMessage = qsTr("Connection deleted")
                    window.refreshConnections()
                } else {
                    window.statusMessage = appController.lastError
                }
            }
        }

        ColumnLayout {
            SplitView.fillWidth: true
            spacing: 0

            SessionTabs {
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                sessions: window.activeSessions
                onSessionClosed: (id) => appController.closeSession(id)
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: window.activeSessions.length > 0 ? 0 : 1

                SplitView {
                    orientation: Qt.Vertical

                    TerminalView {
                        SplitView.fillWidth: true
                        SplitView.fillHeight: true
                        session: window.activeSessions.length > 0 ? window.activeSessions[0] : ({})
                    }

                    FileBrowser {
                        SplitView.fillWidth: true
                        SplitView.preferredHeight: 220
                        session: window.activeSessions.length > 0 ? window.activeSessions[0] : ({})
                    }
                }

                Rectangle {
                    color: "#0f172a"

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 12

                        Label {
                            text: qsTr("No active sessions")
                            color: "#cbd5f5"
                            font.pixelSize: 18
                        }
                        Label {
                            text: qsTr("Pick a connection on the left and press Enter, or double-click it.")
                            color: "#94a3b8"
                            font.pixelSize: 12
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 22
                color: "#020617"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12

                    Label {
                        text: window.statusMessage
                        color: "#94a3b8"
                        font.pixelSize: 11
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                    Label {
                        text: qsTr("Sessions: %1").arg(window.activeSessions.length)
                        color: "#94a3b8"
                        font.pixelSize: 11
                    }
                }
            }
        }
    }

    ConnectionEditor {
        id: connectionEditor
        anchors.centerIn: parent
        modal: true
        connections: window.connections

        onSaved: {
            window.statusMessage = qsTr("Connection saved")
            window.refreshConnections()
        }
        onSaveFailed: (message) => window.statusMessage = message
    }
}
