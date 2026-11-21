import QtQuick

QtObject {
    id: root

    required property var controller
    property var session: ({})
    property bool enabled: false
    property bool includeDetails: true
    property var snapshot: ({})
    property string error: ""
    property string requestId: ""

    readonly property string sessionId: session && session.id ? session.id : ""
    readonly property string connectionId: session && session.connectionId ? session.connectionId : ""
    readonly property bool connected: !!(session && session.status === "connected")
    readonly property bool canPoll: enabled && connected && connectionId.length > 0

    function reset() {
        requestId = ""
        snapshot = ({})
        error = !connected && session && session.lastMessage ? session.lastMessage : ""
    }

    function poll() {
        if (canPoll && requestId.length === 0) {
            requestId = controller.requestSystemMonitorSnapshot(connectionId, includeDetails)
        }
    }

    onSessionIdChanged: {
        reset()
        Qt.callLater(poll)
    }
    onConnectedChanged: {
        if (connected) {
            error = ""
        } else {
            reset()
        }
    }

    readonly property Timer timer: Timer {
        interval: 5000
        repeat: true
        running: root.canPoll
        triggeredOnStart: true
        onTriggered: root.poll()
    }

    readonly property Connections responses: Connections {
        target: root.controller
        function onSystemMonitorSnapshotReady(requestId, connectionId, snapshot, error) {
            if (requestId !== root.requestId || connectionId !== root.connectionId) {
                return
            }
            root.requestId = ""
            root.snapshot = snapshot || ({})
            root.error = error || ""
        }
    }
}
