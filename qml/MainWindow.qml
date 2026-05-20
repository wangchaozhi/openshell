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
    property string activeSessionId: ""
    property string activeView: "connections"
    property string uiTheme: appController.uiTheme
    property string statusMessage: ""
    property var monitorSnapshot: ({})
    property string monitorError: ""
    property string monitorRequestId: ""
    property bool monitorRequestInFlight: false
    property bool fileBrowserVisible: false
    property bool systemInfoTabVisible: false

    readonly property var activeSession: {
        for (let i = 0; i < activeSessions.length; ++i) {
            if (activeSessions[i].id === activeSessionId) {
                return activeSessions[i]
            }
        }
        return ({})
    }

    readonly property string activeConnectionHost: {
        const cid = activeSession.connectionId
        if (!cid) return ""
        for (let i = 0; i < connections.length; ++i) {
            if (connections[i].id === cid) return connections[i].host || ""
        }
        return ""
    }

    function refreshConnections() {
        connections = appController.reloadConnectionProfiles()
    }

    function refreshSessions() {
        activeSessions = appController.sessions()
        // 当前活动会话被外部关掉了，自动落到第一个。
        if (activeSessionId !== "") {
            let stillThere = false
            for (let i = 0; i < activeSessions.length; ++i) {
                if (activeSessions[i].id === activeSessionId) {
                    stillThere = true
                    break
                }
            }
            if (!stillThere) {
                activeSessionId = activeSessions.length > 0 ? activeSessions[0].id : ""
                activeView = activeSessionId.length > 0 ? activeView : "connections"
            }
        } else if (activeSessions.length > 0) {
            activeSessionId = activeSessions[0].id
        }
    }

    function openSessionFor(connectionId) {
        const newId = appController.openSession(connectionId)
        if (newId && newId.length > 0) {
            // 新开的会话自动激活。
            activeSessionId = newId
            activeView = "terminal"
            fileBrowserVisible = false
            delayedFileBrowserLoad.restart()
        } else if (appController.lastError && appController.lastError.length > 0) {
            statusMessage = appController.lastError
        }
    }

    function reconnectSession(sessionId, connectionId) {
        const targetConnectionId = connectionId || ""
        if (targetConnectionId.length === 0) {
            return
        }
        appController.closeSession(sessionId)
        openSessionFor(targetConnectionId)
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
        function onSessionStatusChanged(id, status, message) {
            window.refreshSessions()
            if (message && message.length > 0) {
                window.statusMessage = message
            } else if (status === "connected") {
                window.statusMessage = qsTr("Connected")
            }
            if (id === window.activeSessionId && status !== "connected") {
                window.monitorTimer.stop()
                window.monitorSnapshot = ({})
                window.monitorError = message || ""
                window.monitorRequestId = ""
                window.monitorRequestInFlight = false
            }
        }
        function onLanguageChanged() {
            window.refreshConnections()
        }
        function onUiThemeChanged() {
            window.uiTheme = appController.uiTheme
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

            monitorSnapshot: window.monitorSnapshot
            monitorError: window.monitorError
            uiTheme: window.uiTheme
            hasActiveSession: window.activeSessionId.length > 0
            sessionStatus: window.activeSession && window.activeSession.status ? window.activeSession.status : ""
            connectionHost: window.activeConnectionHost
            onSystemInfoRequested: {
                if (window.activeSessionId.length > 0) {
                    window.activeView = "system"
                    window.systemInfoTabVisible = true
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
                activeSessionId: window.activeSessionId
                activeView: window.activeView
                uiTheme: window.uiTheme
                systemInfoTabVisible: window.systemInfoTabVisible
                onSessionActivated: (id) => {
                    window.activeSessionId = id
                    window.activeView = "terminal"
                    window.fileBrowserVisible = false
                    delayedFileBrowserLoad.restart()
                }
                onConnectionManagerActivated: window.activeView = "connections"
                onSessionClosed: (id) => appController.closeSession(id)
                onSessionDisconnected: (id) => appController.closeSession(id)
                onSessionReconnectRequested: (id, connectionId) => window.reconnectSession(id, connectionId)
                onSystemInfoActivated: window.activeView = "system"
                onSystemInfoClosed: {
                    window.systemInfoTabVisible = false
                    if (window.activeView === "system")
                        window.activeView = "terminal"
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: window.activeView === "connections" ? 0
                              : (window.activeSessions.length === 0 ? 3
                                 : (window.activeView === "system" ? 2 : 1))

                ConnectionManagerView {
                    connections: window.connections
                    selectedConnectionId: window.selectedConnectionId
                    uiTheme: window.uiTheme

                    onConnectionPicked: (id) => window.selectedConnectionId = id
                    onConnectionOpenRequested: (id) => window.openSessionFor(id)
                    onThemeChanged: (theme) => appController.uiTheme = theme
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

                SplitView {
                    orientation: Qt.Vertical

                    TerminalView {
                        id: terminalView
                        SplitView.fillWidth: true
                        SplitView.fillHeight: true
                        session: window.activeSession
                    }

                    Loader {
                        id: fileBrowserLoader
                        SplitView.fillWidth: true
                        SplitView.preferredHeight: 220
                        active: window.fileBrowserVisible && window.activeView === "terminal" && window.activeSessionId.length > 0
                        sourceComponent: FileBrowser {
                            session: window.activeSession
                            terminalRemotePath: terminalView.detectedRemotePath
                        }
                    }
                }

                Loader {
                    active: window.activeView === "system"
                    sourceComponent: SystemInfoView {
                        snapshot: window.monitorSnapshot
                        error: window.monitorError
                    }
                }

                Rectangle {
                    color: window.uiTheme === "classic" ? "#ffffff" : "#0f172a"

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 12

                        Label {
                            text: qsTr("No active sessions")
                            color: window.uiTheme === "classic" ? "#0f172a" : "#cbd5f5"
                            font.pixelSize: 18
                        }
                        Label {
                            text: qsTr("Open the connection manager folder and double-click a connection.")
                            color: window.uiTheme === "classic" ? "#64748b" : "#94a3b8"
                            font.pixelSize: 12
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 22
                color: window.uiTheme === "classic" ? "#f8fafc" : "#020617"
                border.color: window.uiTheme === "classic" ? "#cbd5e1" : "#020617"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12

                    Label {
                        text: window.statusMessage
                        color: window.uiTheme === "classic" ? "#475569" : "#94a3b8"
                        font.pixelSize: 11
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                    Label {
                        text: qsTr("Sessions: %1").arg(window.activeSessions.length)
                        color: window.uiTheme === "classic" ? "#475569" : "#94a3b8"
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
        uiTheme: window.uiTheme

        onSaved: {
            window.statusMessage = qsTr("Connection saved")
            window.refreshConnections()
        }
        onSaveFailed: (message) => window.statusMessage = message
    }

    Timer {
        id: monitorTimer
        interval: 5000
        repeat: true
        running: window.activeSession && window.activeSession.connectionId && window.activeSession.status === "connected"
        triggeredOnStart: true
        onTriggered: {
            if (window.activeSession && window.activeSession.connectionId && !window.monitorRequestInFlight) {
                window.monitorRequestInFlight = true
                window.monitorRequestId = appController.requestSystemMonitorSnapshot(window.activeSession.connectionId)
            }
        }
    }

    onActiveSessionIdChanged: {
        monitorSnapshot = ({})
        monitorError = ""
        monitorRequestId = ""
        monitorRequestInFlight = false
        fileBrowserVisible = false
        systemInfoTabVisible = false
        if (activeView === "system") activeView = "terminal"
        delayedFileBrowserLoad.restart()
        monitorTimer.restart()
    }

    Timer {
        id: delayedFileBrowserLoad
        interval: 80
        repeat: false
        onTriggered: window.fileBrowserVisible = window.activeView === "terminal" && window.activeSessionId.length > 0
    }

    Connections {
        target: appController
        function onSystemMonitorSnapshotReady(requestId, connectionId, snapshot, error) {
            if (requestId !== window.monitorRequestId) {
                return
            }
            window.monitorRequestInFlight = false
            window.monitorSnapshot = snapshot || ({})
            window.monitorError = error || ""
        }
    }
}
