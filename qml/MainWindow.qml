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
    color: theme.window

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
    property string sftpStatus: ""
    property string sftpMessage: ""
    property bool fileBrowserVisible: false
    property bool fileBrowserPanesDetached: false
    property bool systemInfoTabVisible: false
    property var detachedSessionWindows: ({})
    property var detachedSidebarWindow: null

    ThemePalette {
        id: theme
        mode: window.uiTheme
    }

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

    function sessionById(sessionId) {
        for (let i = 0; i < activeSessions.length; ++i) {
            if (activeSessions[i].id === sessionId) {
                return activeSessions[i]
            }
        }
        return ({})
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

    function detachSession(sessionId) {
        if (!sessionId || sessionId.length === 0) {
            return
        }
        const existing = detachedSessionWindows[sessionId]
        if (existing) {
            existing.show()
            existing.raise()
            existing.requestActivate()
            revealDetachedWindow(existing)
            return
        }
        const detached = detachedSessionWindowComponent.createObject(window, {
            detachedSessionId: sessionId
        })
        if (!detached) {
            return
        }
        const next = Object.assign({}, detachedSessionWindows)
        next[sessionId] = detached
        detachedSessionWindows = next
        detached.show()
        detached.raise()
        detached.requestActivate()
        revealDetachedWindow(detached)
    }

    function restoreDetachedSession(sessionId, activate) {
        if (!sessionId || sessionId.length === 0) {
            return
        }
        const next = Object.assign({}, detachedSessionWindows)
        delete next[sessionId]
        detachedSessionWindows = next
        if (activate === undefined || activate) {
            activeSessionId = sessionId
            activeView = "terminal"
        }
    }

    function positionDetachedWindow(detachedWindow, sourceItem) {
        if (!detachedWindow || !sourceItem || !sourceItem.mapToItem) {
            return
        }
        const point = sourceItem.mapToItem(null, sourceItem.width / 2, sourceItem.height / 2)
        detachedWindow.x = Math.round(window.x + point.x - 32)
        detachedWindow.y = Math.round(window.y + point.y - 24)
    }

    function moveDetachedWindow(detachedWindow, sourceItem, translation) {
        if (!detachedWindow || !sourceItem || !sourceItem.mapToItem) {
            return
        }
        const point = sourceItem.mapToItem(null, sourceItem.width / 2, sourceItem.height / 2)
        detachedWindow.x = Math.round(window.x + point.x + translation.x - 32)
        detachedWindow.y = Math.round(window.y + point.y + translation.y - 24)
    }

    function revealDetachedWindow(detachedWindow) {
        if (detachedWindow && detachedWindow.playDetachPop) {
            detachedWindow.playDetachPop()
        }
    }

    // 拖拽过程中把新窗口交给系统窗口管理器，由它跟随仍按住的鼠标移动。
    // 必须等窗口渲染出第一帧后再进入系统移动循环，否则该模态循环会
    // 卡住渲染线程，只剩一个还没画出内容的空框跟着鼠标走。
    function followCursorWithWindow(detachedWindow) {
        if (detachedWindow && detachedWindow.startSystemMove) {
            detachedWindow.pendingSystemMove = true
        }
    }

    function detachSidebar(sourceItem) {
        if (detachedSidebarWindow) {
            positionDetachedWindow(detachedSidebarWindow, sourceItem)
            detachedSidebarWindow.show()
            detachedSidebarWindow.raise()
            detachedSidebarWindow.requestActivate()
            followCursorWithWindow(detachedSidebarWindow)
            return
        }
        detachedSidebarWindow = detachedSidebarWindowComponent.createObject(window)
        if (!detachedSidebarWindow) {
            return
        }
        positionDetachedWindow(detachedSidebarWindow, sourceItem)
        detachedSidebarWindow.show()
        detachedSidebarWindow.raise()
        detachedSidebarWindow.requestActivate()
        followCursorWithWindow(detachedSidebarWindow)
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

    Component {
        id: detachedSessionWindowComponent

        Window {
            id: detachedWindow
            property string detachedSessionId: ""
            readonly property var detachedSession: window.sessionById(detachedSessionId)
            property real detachPopProgress: 1

            function playDetachPop() {
                detachPopProgress = 0
                detachedSessionPop.restart()
            }

            width: 920
            height: 560
            minimumWidth: 640
            minimumHeight: 360
            visible: false
            title: detachedSession && detachedSession.title
                   ? detachedSession.title + " - " + qsTr("OpenShell")
                   : qsTr("OpenShell")
            color: theme.panel

            TerminalView {
                anchors.fill: parent
                session: detachedWindow.detachedSession
                uiTheme: window.uiTheme
                opacity: 0.55 + detachedWindow.detachPopProgress * 0.45
                scale: 0.96 + detachedWindow.detachPopProgress * 0.04
                transformOrigin: Item.Center
            }

            NumberAnimation {
                id: detachedSessionPop
                target: detachedWindow
                property: "detachPopProgress"
                from: 0
                to: 1
                duration: 150
                easing.type: Easing.OutCubic
            }

            onClosing: function(close) {
                window.restoreDetachedSession(detachedSessionId, true)
                close.accepted = true
                destroy()
            }

            Connections {
                target: appController
                function onSessionsChanged() {
                    if (!window.sessionById(detachedWindow.detachedSessionId).id) {
                        detachedWindow.destroy()
                    }
                }
            }

            Component.onDestruction: window.restoreDetachedSession(detachedSessionId, false)
        }
    }

    Component {
        id: detachedSidebarWindowComponent

        Window {
            id: sidebarWindow
            property bool pendingSystemMove: false

            onFrameSwapped: {
                if (pendingSystemMove) {
                    pendingSystemMove = false
                    startSystemMove()
                }
            }

            width: 300
            height: 720
            minimumWidth: 240
            minimumHeight: 420
            visible: false
            title: qsTr("Sidebar") + " - " + qsTr("OpenShell")
            color: theme.window

            Sidebar {
                anchors.fill: parent
                monitorSnapshot: window.monitorSnapshot
                monitorError: window.monitorError
                uiTheme: window.uiTheme
                hasActiveSession: window.activeSessionId.length > 0
                sessionStatus: window.activeSession && window.activeSession.status ? window.activeSession.status : ""
                sftpStatus: window.sftpStatus
                sftpMessage: window.sftpMessage
                connectionHost: window.activeConnectionHost
                onSystemInfoRequested: {
                    if (window.activeSessionId.length > 0) {
                        window.activeView = "system"
                        window.systemInfoTabVisible = true
                    }
                }
                onDetachRequested: (sourceItem) => window.detachSidebar(sourceItem)
                onDetachDragged: (sourceItem, translation) => {
                    window.moveDetachedWindow(window.detachedSidebarWindow,
                                              sourceItem,
                                              translation)
                }
            }

            onClosing: function(close) {
                close.accepted = true
                destroy()
            }

            Component.onDestruction: {
                if (window.detachedSidebarWindow === sidebarWindow) {
                    window.detachedSidebarWindow = null
                }
            }
        }
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
                monitorTimer.stop()
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
            visible: !window.detachedSidebarWindow
            SplitView.preferredWidth: window.detachedSidebarWindow ? 0 : 280
            SplitView.minimumWidth: window.detachedSidebarWindow ? 0 : 220

            monitorSnapshot: window.monitorSnapshot
            monitorError: window.monitorError
            uiTheme: window.uiTheme
            hasActiveSession: window.activeSessionId.length > 0
            sessionStatus: window.activeSession && window.activeSession.status ? window.activeSession.status : ""
            sftpStatus: window.sftpStatus
            sftpMessage: window.sftpMessage
            connectionHost: window.activeConnectionHost
            onDetachRequested: (sourceItem) => window.detachSidebar(sourceItem)
            onDetachDragged: (sourceItem, translation) => {
                window.moveDetachedWindow(window.detachedSidebarWindow,
                                          sourceItem,
                                          translation)
            }
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
                onSessionDetached: (id) => window.detachSession(id)
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
                        uiTheme: window.uiTheme
                    }

                    Loader {
                        id: fileBrowserLoader
                        SplitView.fillWidth: true
                        SplitView.preferredHeight: window.fileBrowserPanesDetached ? 0 : 220
                        SplitView.minimumHeight: window.fileBrowserPanesDetached ? 0 : 120
                        visible: !window.fileBrowserPanesDetached
                        active: window.fileBrowserVisible && window.activeView === "terminal" && window.activeSessionId.length > 0
                        sourceComponent: FileBrowser {
                            session: window.activeSession
                            uiTheme: window.uiTheme
                            terminalRemotePath: terminalView.detectedRemotePath
                            onSftpStatusChanged: (status, message) => {
                                window.sftpStatus = status
                                window.sftpMessage = message || ""
                            }
                            onPanesDetachedChanged: (bothDetached) => {
                                window.fileBrowserPanesDetached = bothDetached
                            }
                        }
                    }
                }

                Loader {
                    active: window.activeView === "system"
                    sourceComponent: SystemInfoView {
                        snapshot: window.monitorSnapshot
                        error: window.monitorError
                        uiTheme: window.uiTheme
                    }
                }

                Rectangle {
                    color: theme.surface

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 12

                        Label {
                            text: qsTr("No active sessions")
                            color: theme.textPrimary
                            font.pixelSize: 18
                        }
                        Label {
                            text: qsTr("Open the connection manager folder and double-click a connection.")
                            color: theme.textMuted
                            font.pixelSize: 12
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 22
                color: theme.surface
                border.color: theme.border

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12

                    Label {
                        text: window.statusMessage
                        color: theme.textMuted
                        font.pixelSize: 11
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                    Label {
                        text: qsTr("Sessions: %1").arg(window.activeSessions.length)
                        color: theme.textMuted
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
        running: !!(window.activeSession && window.activeSession.connectionId && window.activeSession.status === "connected")
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
        sftpStatus = ""
        sftpMessage = ""
        fileBrowserVisible = false
        fileBrowserPanesDetached = false
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
