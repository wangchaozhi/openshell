import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: window

    width: 430
    height: 760
    visible: true
    title: qsTr("OpenShell")
    color: "#0b1220"
    // 整个移动端走 Material Dark：默认 Light 主题在我们 #0b1220 的深底上
    // 让 TextField / ComboBox 的文字几乎不可见。
    Material.theme: Material.Dark
    Material.accent: "#38bdf8"
    Material.primary: "#1e3a8a"
    Material.background: "#0b1220"
    Material.foreground: "#f8fafc"

    property var connections: appController.connectionProfiles()
    property var activeSessions: appController.sessions()
    property string activePage: "connections"
    property string activeSessionId: ""
    property string editingConnectionId: ""
    property string statusMessage: ""
    property var monitorSnapshot: ({})
    property string monitorError: ""
    property string monitorRequestId: ""
    property bool monitorRequestInFlight: false

    readonly property var activeSession: {
        for (let i = 0; i < activeSessions.length; ++i) {
            if (activeSessions[i].id === activeSessionId) {
                return activeSessions[i]
            }
        }
        return ({})
    }

    function refreshConnections() {
        connections = appController.reloadConnectionProfiles()
    }

    function refreshSessions() {
        activeSessions = appController.sessions()
        if (activeSessionId.length === 0 && activeSessions.length > 0) {
            activeSessionId = activeSessions[0].id
        }
    }

    function openConnection(connectionId) {
        const sessionId = appController.openSession(connectionId)
        if (sessionId && sessionId.length > 0) {
            activeSessionId = sessionId
            activePage = "terminal"
            statusMessage = qsTr("Session opened")
        } else {
            statusMessage = appController.lastError
        }
    }

    function profileById(id) {
        for (let i = 0; i < connections.length; ++i) {
            if (connections[i].id === id) {
                return connections[i]
            }
        }
        return ({})
    }

    function newConnection() {
        editingConnectionId = ""
        activePage = "editor"
        editorPage.openForNew()
    }

    function editConnection(id) {
        editingConnectionId = id
        activePage = "editor"
        editorPage.openForEdit(profileById(id))
    }

    function deleteConnection(id) {
        if (appController.deleteConnection(id)) {
            statusMessage = qsTr("Connection deleted")
            refreshConnections()
        } else {
            statusMessage = appController.lastError
        }
    }

    Connections {
        target: appController

        function onSessionsChanged() {
            window.refreshSessions()
        }

        function onSessionStatusChanged(id, status, message) {
            window.refreshSessions()
            if (message && message.length > 0) {
                window.statusMessage = message
            }
        }

        function onSystemMonitorSnapshotReady(requestId, connectionId, snapshot, error) {
            if (requestId !== window.monitorRequestId) {
                return
            }
            window.monitorRequestInFlight = false
            window.monitorSnapshot = snapshot || ({})
            window.monitorError = error || ""
        }
    }

    Timer {
        id: monitorTimer
        interval: 5000
        repeat: true
        running: !!(window.activeSession && window.activeSession.connectionId)
        triggeredOnStart: true
        onTriggered: {
            if (window.activeSession && window.activeSession.connectionId && !window.monitorRequestInFlight) {
                window.monitorRequestInFlight = true
                window.monitorRequestId = appController.requestSystemMonitorSnapshot(window.activeSession.connectionId)
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            color: "#111827"

            Label {
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("OpenShell")
                color: "#f8fafc"
                font.pixelSize: 20
                font.bold: true
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: window.activePage === "connections" ? 0
                          : (window.activePage === "terminal" ? 1
                             : (window.activePage === "system" ? 2
                                : (window.activePage === "files" ? 3 : 4)))

            MobileConnectionsPage {
                connections: window.connections
                statusMessage: window.statusMessage
                onRefreshRequested: window.refreshConnections()
                onNewConnectionRequested: window.newConnection()
                onConnectionEditRequested: (id) => window.editConnection(id)
                onConnectionDeleteRequested: (id) => window.deleteConnection(id)
                onConnectionOpenRequested: (id) => window.openConnection(id)
            }

            MobileTerminalPage {
                session: window.activeSession
                onBackRequested: window.activePage = "connections"
            }

            MobileSystemPage {
                snapshot: window.monitorSnapshot
                error: window.monitorError
                hasSession: window.activeSessionId.length > 0
            }

            MobileFilesPage {
                hasSession: window.activeSessionId.length > 0
                connectionId: window.activeSession && window.activeSession.connectionId
                              ? window.activeSession.connectionId : ""
            }

            MobileConnectionEditorPage {
                id: editorPage
                onSaved: {
                    window.statusMessage = qsTr("Connection saved")
                    window.refreshConnections()
                    window.activePage = "connections"
                }
                onCanceled: window.activePage = "connections"
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
            visible: window.activePage !== "editor"
            color: "#111827"
            border.color: "#1e293b"

            RowLayout {
                anchors.fill: parent
                spacing: 0

                Repeater {
                    model: [
                        { key: "connections", label: qsTr("Connections") },
                        { key: "terminal", label: qsTr("Terminal") },
                        { key: "system", label: qsTr("System") },
                        { key: "files", label: qsTr("Files") }
                    ]

                    // 自绘 tab：Material Button 的水平 padding ~48dp，单 tab 宽 ~95dp
                    // 时会把标签 elide 成 "Conn..."；这里直接用 Item + Label 控制。
                    delegate: Item {
                        required property var modelData

                        readonly property bool tabEnabled: modelData.key === "connections"
                                                          || window.activeSessionId.length > 0
                        readonly property bool tabActive: window.activePage === modelData.key

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        opacity: tabEnabled ? 1.0 : 0.35

                        Rectangle {
                            anchors.fill: parent
                            anchors.topMargin: 6
                            anchors.bottomMargin: 6
                            anchors.leftMargin: 4
                            anchors.rightMargin: 4
                            radius: 10
                            color: tabMouse.pressed
                                   ? "#1d4ed8"
                                   : (parent.tabActive ? "#1e3a8a" : "transparent")
                        }

                        Label {
                            anchors.fill: parent
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            text: parent.modelData.label
                            color: parent.tabActive ? "#f8fafc" : "#cbd5f5"
                            font.pixelSize: 12
                            font.bold: parent.tabActive
                            elide: Text.ElideNone
                        }

                        MouseArea {
                            id: tabMouse
                            anchors.fill: parent
                            enabled: parent.tabEnabled
                            onClicked: window.activePage = parent.modelData.key
                        }
                    }
                }
            }
        }
    }
}
