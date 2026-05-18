import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var connections: []
    property string selectedConnectionId: ""
    property var monitorSnapshot: ({})
    property string monitorError: ""

    signal connectionPicked(string id)
    signal connectionDoublePicked(string id)
    signal newConnectionRequested()
    signal editConnectionRequested(string id)
    signal deleteConnectionRequested(string id)

    color: "#0b1220"
    border.color: "#1e293b"

    component PlusIcon: Item {
        implicitWidth: 14
        implicitHeight: 14
        Rectangle {
            anchors.centerIn: parent
            width: 14
            height: 2
            radius: 1
            color: "#dbeafe"
        }
        Rectangle {
            anchors.centerIn: parent
            width: 2
            height: 14
            radius: 1
            color: "#dbeafe"
        }
    }

    component ProtocolIcon: Item {
        property string protocol: "ssh"

        implicitWidth: 16
        implicitHeight: 16

        Rectangle {
            visible: protocol === "ssh"
            anchors.centerIn: parent
            width: 12
            height: 8
            radius: 2
            color: "#38bdf8"
        }
        Rectangle {
            visible: protocol === "ssh"
            x: 4
            y: 2
            width: 8
            height: 8
            radius: 4
            color: "transparent"
            border.color: "#38bdf8"
            border.width: 2
        }
        Rectangle {
            visible: protocol === "sftp"
            x: 1
            y: 4
            width: 14
            height: 10
            radius: 2
            color: "#93c5fd"
        }
        Rectangle {
            visible: protocol === "sftp"
            x: 2
            y: 2
            width: 7
            height: 4
            radius: 1
            color: "#60a5fa"
        }
        Rectangle {
            visible: protocol !== "ssh" && protocol !== "sftp"
            anchors.centerIn: parent
            width: 14
            height: 10
            radius: 2
            color: "transparent"
            border.color: "#a78bfa"
            border.width: 2
        }
    }

    component MeterBar: Item {
        property real value: 0
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
            radius: 2
            color: "#111827"
            border.color: "#334155"
        }
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width * clampedValue() / 100
            radius: 2
            color: clampedValue() >= 85 ? "#ef4444" : (clampedValue() >= 65 ? "#f59e0b" : "#22c55e")
        }
        Label {
            anchors.centerIn: parent
            text: clampedValue().toFixed(1) + "%"
            color: "#e2e8f0"
            font.pixelSize: 10
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: qsTr("Connections")
                color: "#f1f5f9"
                font.bold: true
                font.pixelSize: 14
                Layout.fillWidth: true
            }

            ToolButton {
                implicitWidth: 34
                implicitHeight: 30
                contentItem: PlusIcon {
                    anchors.centerIn: parent
                }
                ToolTip.visible: hovered
                ToolTip.text: qsTr("New connection")
                onClicked: root.newConnectionRequested()
            }
        }

        TextField {
            id: searchField
            Layout.fillWidth: true
            placeholderText: qsTr("Filter…")
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(160, parent.height * 0.48)
            Layout.fillHeight: true
            clip: true
            spacing: 4

            model: root.connections.filter(function(p) {
                if (searchField.text.length === 0) return true
                const q = searchField.text.toLowerCase()
                return (p.name || "").toLowerCase().indexOf(q) >= 0
                        || (p.host || "").toLowerCase().indexOf(q) >= 0
                        || (p.username || "").toLowerCase().indexOf(q) >= 0
            })

            delegate: Rectangle {
                width: list.width
                height: 56
                radius: 6
                color: modelData.id === root.selectedConnectionId ? "#1e3a8a" : "transparent"
                border.color: modelData.id === root.selectedConnectionId ? "#3b82f6" : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8

                    ProtocolIcon {
                        protocol: (modelData.protocol || "ssh").toLowerCase()
                        Layout.alignment: Qt.AlignVCenter
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            text: modelData.name || qsTr("(unnamed)")
                            color: "#f8fafc"
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Label {
                            text: (modelData.protocol || "ssh").toUpperCase()
                                  + "  " + (modelData.username || "")
                                  + "@" + (modelData.host || "?")
                                  + ":" + (modelData.port || 22)
                            color: "#94a3b8"
                            font.pixelSize: 11
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: function(mouse) {
                        root.connectionPicked(modelData.id)
                        if (mouse.button === Qt.RightButton) {
                            contextMenu.popup()
                        }
                    }
                    onDoubleClicked: root.connectionDoublePicked(modelData.id)

                    Menu {
                        id: contextMenu
                        MenuItem {
                            text: qsTr("Open Session")
                            onTriggered: root.connectionDoublePicked(modelData.id)
                        }
                        MenuItem {
                            text: qsTr("Edit")
                            onTriggered: root.editConnectionRequested(modelData.id)
                        }
                        MenuItem {
                            text: qsTr("Delete")
                            onTriggered: root.deleteConnectionRequested(modelData.id)
                        }
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: root.connections.length === 0
            text: qsTr("No connections yet. Press + to add one.")
            color: "#64748b"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }

        Rectangle {
            id: monitorPanel

            Layout.fillWidth: true
            Layout.preferredHeight: 230
            color: "#020617"
            border.color: "#1e293b"
            radius: 6

            readonly property var cpu: root.monitorSnapshot && root.monitorSnapshot.cpu ? root.monitorSnapshot.cpu : ({})
            readonly property var memory: root.monitorSnapshot && root.monitorSnapshot.memory ? root.monitorSnapshot.memory : ({})
            readonly property var processes: root.monitorSnapshot && root.monitorSnapshot.processes ? root.monitorSnapshot.processes : []
            readonly property var filesystems: root.monitorSnapshot && root.monitorSnapshot.filesystems ? root.monitorSnapshot.filesystems : []

            function percentValue(source, key) {
                const current = Number(source && source[key] !== undefined ? source[key] : 0)
                return isFinite(current) ? current : 0
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 7

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: qsTr("Current server")
                        color: "#e2e8f0"
                        font.bold: true
                        font.pixelSize: 12
                        Layout.fillWidth: true
                    }
                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: root.monitorError.length > 0 ? "#f87171"
                              : root.monitorSnapshot.updatedAt ? "#22c55e" : "#64748b"
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: root.monitorError.length > 0
                          ? root.monitorError
                          : (root.monitorSnapshot.updatedAt ? qsTr("Monitor online") : qsTr("Open a session to monitor."))
                    color: root.monitorError.length > 0 ? "#fca5a5" : "#94a3b8"
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 8
                    rowSpacing: 5

                    Label { text: qsTr("CPU"); color: "#93c5fd"; font.pixelSize: 11 }
                    MeterBar {
                        Layout.fillWidth: true
                        value: monitorPanel.percentValue(monitorPanel.cpu, "busyPercent")
                    }
                    Label { text: qsTr("Mem"); color: "#93c5fd"; font.pixelSize: 11 }
                    MeterBar {
                        Layout.fillWidth: true
                        value: monitorPanel.percentValue(monitorPanel.memory, "memUsedPercent")
                    }
                    Label { text: qsTr("Swap"); color: "#93c5fd"; font.pixelSize: 11 }
                    MeterBar {
                        Layout.fillWidth: true
                        value: monitorPanel.percentValue(monitorPanel.memory, "swapUsedPercent")
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Memory %1 / %2").arg(monitorPanel.memory.memUsedText || "--").arg(monitorPanel.memory.memTotalText || "--")
                    color: "#cbd5e1"
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: monitorPanel.filesystems.length > 0
                          ? qsTr("Disk %1 free at %2").arg(monitorPanel.filesystems[0].available || "--").arg(monitorPanel.filesystems[0].mount || "/")
                          : qsTr("Disk --")
                    color: "#cbd5e1"
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }

                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#1e293b" }

                Repeater {
                    model: monitorPanel.processes.slice(0, 4)
                    delegate: RowLayout {
                        Layout.fillWidth: true
                        Label {
                            text: modelData.name || ""
                            color: "#dbeafe"
                            font.pixelSize: 11
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                        Label {
                            text: (modelData.cpu || 0).toFixed(1) + "%"
                            color: "#fca5a5"
                            font.pixelSize: 11
                        }
                    }
                }
            }
        }
    }
}
