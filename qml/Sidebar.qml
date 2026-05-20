import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var monitorSnapshot: ({})
    property string monitorError: ""
    property string uiTheme: "dark"
    property bool hasActiveSession: false
    property string sessionStatus: ""
    property string sftpStatus: ""
    property string sftpMessage: ""
    property string connectionHost: ""
    readonly property bool classic: uiTheme === "classic"
    readonly property bool sessionConnected: sessionStatus === "connected"
    readonly property bool sessionProblem: sessionStatus === "disconnected" || sessionStatus === "error"
    readonly property bool sftpConnected: sftpStatus === "connected"
    readonly property bool sftpProblem: sftpStatus === "error"

    signal systemInfoRequested()

    color: classic ? "#f8fafc" : "#0b1220"
    border.color: classic ? "#cbd5e1" : "#1e293b"

    component MeterBar: Item {
        property real value: 0
        property string detailText: ""
        implicitHeight: 16

        function clampedValue() {
            const current = Number(value)
            if (!isFinite(current)) {
                return 0
            }
            return Math.max(0, Math.min(100, current))
        }

        Rectangle {
            anchors.fill: parent
            color: root.classic ? "#f1f5f9" : "#111827"
            border.color: root.classic ? "#cbd5e1" : "#334155"
        }
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width * clampedValue() / 100
            color: clampedValue() >= 85 ? "#ef4444"
                   : (clampedValue() >= 65 ? "#f59e0b" : (root.classic ? "#bbf7d0" : "#22c55e"))
        }
        Label {
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            text: clampedValue().toFixed(1) + "%"
            color: root.classic ? "#334155" : "#e2e8f0"
            font.pixelSize: 10
        }
        Label {
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            text: detailText
            color: root.classic ? "#334155" : "#e2e8f0"
            font.pixelSize: 10
            elide: Text.ElideRight
        }
    }

    ColumnLayout {
        id: monitorColumn

        anchors.fill: parent
        anchors.margins: 6
        spacing: 8

        readonly property var cpu: root.monitorSnapshot && root.monitorSnapshot.cpu ? root.monitorSnapshot.cpu : ({})
        readonly property var memory: root.monitorSnapshot && root.monitorSnapshot.memory ? root.monitorSnapshot.memory : ({})
        readonly property var processes: root.monitorSnapshot && root.monitorSnapshot.processes ? root.monitorSnapshot.processes : []
        readonly property var filesystems: root.monitorSnapshot && root.monitorSnapshot.filesystems ? root.monitorSnapshot.filesystems : []

        function percentValue(source, key) {
            const current = Number(source && source[key] !== undefined ? source[key] : 0)
            return isFinite(current) ? current : 0
        }

        function statusColor(status, connected, problem) {
            if (problem) {
                return "#ef4444"
            }
            if (connected) {
                return "#22c55e"
            }
            if (status === "connecting" || root.hasActiveSession) {
                return "#f59e0b"
            }
            return "#94a3b8"
        }

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: qsTr("Status")
                color: root.classic ? "#0f172a" : "#e2e8f0"
                font.pixelSize: 12
                Layout.fillWidth: true
            }
            Label {
                text: "SSH"
                color: root.classic ? "#64748b" : "#94a3b8"
                font.pixelSize: 10
            }
            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: monitorColumn.statusColor(root.sessionStatus, root.sessionConnected, root.sessionProblem)
            }
            Label {
                text: "SFTP"
                color: root.classic ? "#64748b" : "#94a3b8"
                font.pixelSize: 10
            }
            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: monitorColumn.statusColor(root.sftpStatus, root.sftpConnected, root.sftpProblem)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            Label {
                text: qsTr("IP")
                color: root.classic ? "#0f172a" : "#93c5fd"
                font.pixelSize: 11
            }
            Label {
                id: ipLabel
                Layout.fillWidth: true
                text: root.connectionHost.length > 0 ? root.connectionHost : "-"
                color: root.classic ? "#0f172a" : "#cbd5e1"
                font.pixelSize: 11
                elide: Text.ElideRight
            }
            ToolButton {
                implicitWidth: 20
                implicitHeight: 20
                visible: ipLabel.text !== "-"
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Copy IP")
                contentItem: Item {
                    Rectangle {
                        x: 3; y: 5; width: 10; height: 8
                        color: "transparent"
                        border.color: root.classic ? "#64748b" : "#94a3b8"
                        border.width: 1.5
                        radius: 1
                    }
                    Rectangle {
                        x: 6; y: 3; width: 10; height: 8
                        color: root.classic ? "#f8fafc" : "#0b1220"
                        border.color: root.classic ? "#64748b" : "#94a3b8"
                        border.width: 1.5
                        radius: 1
                    }
                }
                background: Rectangle {
                    radius: 3
                    color: parent.hovered ? (root.classic ? "#e2e8f0" : "#1e293b") : "transparent"
                }
                onClicked: appController.copyTextToClipboard(ipLabel.text)
            }
        }

        Label {
            Layout.fillWidth: true
            text: root.monitorError.length > 0
                  ? root.monitorError
                  : root.sessionProblem ? qsTr("SSH disconnected")
                  : root.sftpProblem ? (root.sftpMessage.length > 0 ? root.sftpMessage : qsTr("SFTP disconnected"))
                  : (root.monitorSnapshot.updatedAt ? qsTr("Monitor online") : qsTr("Open a session to monitor."))
            color: root.monitorError.length > 0 || root.sessionProblem || root.sftpProblem ? "#fca5a5" : (root.classic ? "#64748b" : "#94a3b8")
            font.pixelSize: 11
            elide: Text.ElideRight
        }

        Button {
            Layout.fillWidth: true
            text: qsTr("System Information")
            enabled: root.hasActiveSession
            onClicked: root.systemInfoRequested()
            implicitHeight: 28
            background: Rectangle {
                radius: 4
                color: !parent.enabled ? (root.classic ? "#f1f5f9" : "#0f172a")
                       : parent.hovered ? (root.classic ? "#e0f2fe" : "#1e3a5f")
                       : (root.classic ? "#ffffff" : "#1e293b")
                border.color: root.classic ? (parent.enabled ? "#cbd5e1" : "#e2e8f0")
                                           : (parent.enabled ? "#475569" : "#1e293b")
            }
            contentItem: Label {
                text: parent.text
                color: !parent.enabled ? (root.classic ? "#94a3b8" : "#475569")
                       : (root.classic ? "#334155" : "#cbd5e1")
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 12
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 6
            rowSpacing: 4

            Label { text: qsTr("CPU"); color: root.classic ? "#0f172a" : "#93c5fd"; font.pixelSize: 11 }
            MeterBar {
                Layout.fillWidth: true
                value: monitorColumn.percentValue(monitorColumn.cpu, "busyPercent")
            }
            Label { text: qsTr("Mem"); color: root.classic ? "#0f172a" : "#93c5fd"; font.pixelSize: 11 }
            MeterBar {
                Layout.fillWidth: true
                value: monitorColumn.percentValue(monitorColumn.memory, "memUsedPercent")
                detailText: (monitorColumn.memory.memUsedText || "--") + "/" + (monitorColumn.memory.memTotalText || "--")
            }
            Label { text: qsTr("Swap"); color: root.classic ? "#0f172a" : "#93c5fd"; font.pixelSize: 11 }
            MeterBar {
                Layout.fillWidth: true
                value: monitorColumn.percentValue(monitorColumn.memory, "swapUsedPercent")
                detailText: (monitorColumn.memory.swapUsedText || "--") + "/" + (monitorColumn.memory.swapTotalText || "--")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 1
            Label {
                text: qsTr("Memory")
                color: "#ffffff"
                horizontalAlignment: Text.AlignHCenter
                background: Rectangle { color: "#60a5fa" }
                Layout.preferredWidth: 66
            }
            Label {
                text: qsTr("CPU")
                color: "#ffffff"
                horizontalAlignment: Text.AlignHCenter
                background: Rectangle { color: "#ef4444" }
                Layout.preferredWidth: 42
            }
            Label {
                text: qsTr("Command")
                color: "#ffffff"
                horizontalAlignment: Text.AlignHCenter
                background: Rectangle { color: "#60a5fa" }
                Layout.fillWidth: true
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 96
            color: root.classic ? "#f8fafc" : "#020617"
            border.color: root.classic ? "#cbd5e1" : "#1e293b"

            ListView {
                id: processList

                anchors.fill: parent
                clip: true
                model: monitorColumn.processes.slice(0, 4)
                delegate: RowLayout {
                    width: ListView.view.width
                    height: 22
                    Label {
                        text: modelData.rss || "0"
                        color: root.classic ? "#334155" : "#cbd5e1"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignRight
                        Layout.preferredWidth: 66
                    }
                    Label {
                        text: (modelData.cpu || 0).toFixed(1)
                        color: root.classic ? "#334155" : "#cbd5e1"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignRight
                        Layout.preferredWidth: 42
                    }
                    Label {
                        text: modelData.name || ""
                        color: root.classic ? "#334155" : "#dbeafe"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: processList.count === 0
                text: root.monitorSnapshot.updatedAt ? qsTr("No process data") : qsTr("Waiting for monitor data")
                color: root.classic ? "#94a3b8" : "#64748b"
                font.pixelSize: 11
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: root.classic ? "#cbd5e1" : "#1e293b"
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Path")
            color: root.classic ? "#0f172a" : "#e2e8f0"
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 12
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: root.classic ? "#ffffff" : "#020617"
            border.color: root.classic ? "#cbd5e1" : "#1e293b"

            ListView {
                id: filesystemList

                anchors.fill: parent
                clip: true
                model: monitorColumn.filesystems
                delegate: RowLayout {
                    width: ListView.view.width
                    height: 24
                    Label {
                        text: modelData.mount || ""
                        color: root.classic ? "#0f172a" : "#cbd5e1"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: (modelData.available || "--") + "/" + (modelData.size || "--")
                        color: root.classic ? "#0f172a" : "#cbd5e1"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignRight
                        Layout.preferredWidth: 110
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: filesystemList.count === 0
                text: root.monitorSnapshot.updatedAt ? qsTr("No file system data") : qsTr("Waiting for monitor data")
                color: root.classic ? "#94a3b8" : "#64748b"
                font.pixelSize: 11
            }
        }
    }
}
