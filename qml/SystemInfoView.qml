import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var snapshot: ({})
    property string error: ""

    color: "#020617"

    readonly property var info: snapshot.info || ({})
    readonly property var cpu: snapshot.cpu || ({})
    readonly property var cpuDetails: snapshot.cpuDetails || []
    readonly property var memory: snapshot.memory || ({})
    readonly property var networks: snapshot.networks || []
    readonly property var filesystems: snapshot.filesystems || []
    readonly property var processes: snapshot.processes || []

    function pct(value) {
        return (Number(value || 0)).toFixed(1) + "%"
    }

    function row(label, value) {
        return { label: label, value: value || "--" }
    }

    Flickable {
        anchors.fill: parent
        anchors.margins: 18
        contentWidth: width
        contentHeight: contentColumn.implicitHeight
        clip: true

        ColumnLayout {
            id: contentColumn
            width: parent.width
            spacing: 18

            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: qsTr("System Information")
                    color: "#f8fafc"
                    font.pixelSize: 20
                    font.bold: true
                    Layout.fillWidth: true
                }
                Label {
                    text: root.error.length > 0 ? root.error
                          : (root.snapshot.updatedAt ? qsTr("Updated %1").arg(root.snapshot.updatedAt) : qsTr("Waiting for monitor data"))
                    color: root.error.length > 0 ? "#fca5a5" : "#94a3b8"
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 34
                rowSpacing: 10

                Repeater {
                    model: [
                        root.row(qsTr("Operating system"), root.info.os),
                        root.row(qsTr("Kernel"), root.info.kernel),
                        root.row(qsTr("Kernel release"), root.info.kernel_release),
                        root.row(qsTr("Architecture"), root.info.arch),
                        root.row(qsTr("Hostname"), root.info.hostname),
                        root.row(qsTr("Uptime"), root.info.uptime)
                    ]
                    delegate: RowLayout {
                        Layout.fillWidth: true
                        Label {
                            text: modelData.label
                            color: "#93c5fd"
                            font.pixelSize: 12
                            Layout.preferredWidth: 150
                        }
                        Label {
                            text: modelData.value
                            color: "#e2e8f0"
                            font.pixelSize: 13
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#1e293b" }

            GridLayout {
                Layout.fillWidth: true
                columns: 4
                columnSpacing: 24
                rowSpacing: 8

                Label { text: qsTr("CPU"); color: "#f8fafc"; font.bold: true; font.pixelSize: 14 }
                Label { text: root.cpu.model || "--"; color: "#e2e8f0"; Layout.columnSpan: 3; Layout.fillWidth: true; elide: Text.ElideRight }
            }

            SectionTable {
                title: qsTr("CPU Details")
                headers: [qsTr("Name"), qsTr("Cores"), qsTr("Frequency"), qsTr("Cache"), qsTr("BogoMips")]
                columnWeights: [2.6, 0.55, 0.9, 0.9, 0.9]
                numericColumns: [1, 2, 4]
                rows: root.cpuDetails.map(function(c) {
                    return [c.name, c.cores, c.frequency, c.cache, c.bogomips]
                })
            }

            SectionTable {
                title: qsTr("CPU Usage")
                headers: [qsTr("User"), qsTr("System"), qsTr("Nice"), qsTr("Idle"), qsTr("IO"), qsTr("Hardware IRQ"), qsTr("Software IRQ"), qsTr("Realtime")]
                columnWeights: [0.75, 0.75, 0.65, 0.75, 0.6, 1.05, 1.05, 0.8]
                numericColumns: [0, 1, 2, 3, 4, 5, 6, 7]
                rows: [[
                    root.pct(root.cpu.userPercent),
                    root.pct(root.cpu.systemPercent),
                    root.pct(root.cpu.nicePercent),
                    root.pct(root.cpu.idlePercent),
                    root.pct(root.cpu.ioPercent),
                    root.pct(root.cpu.irqPercent),
                    root.pct(root.cpu.softirqPercent),
                    root.pct(root.cpu.busyPercent)
                ]]
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 4
                columnSpacing: 24
                rowSpacing: 8

                Label { text: qsTr("Memory"); color: "#f8fafc"; font.bold: true; font.pixelSize: 14 }
                Label { text: qsTr("Used %1 / %2").arg(root.memory.memUsedText || "--").arg(root.memory.memTotalText || "--"); color: "#e2e8f0"; Layout.columnSpan: 3 }
                Label { text: qsTr("Memory used"); color: "#93c5fd"; font.pixelSize: 12 }
                Label { text: root.pct(root.memory.memUsedPercent); color: "#e2e8f0" }
                Label { text: qsTr("Available"); color: "#93c5fd"; font.pixelSize: 12 }
                Label { text: root.memory.memAvailableText || "--"; color: "#e2e8f0" }
                Label { text: qsTr("Swap used"); color: "#93c5fd"; font.pixelSize: 12 }
                Label { text: root.pct(root.memory.swapUsedPercent); color: "#e2e8f0" }
                Label { text: qsTr("Swap"); color: "#93c5fd"; font.pixelSize: 12 }
                Label { text: qsTr("%1 / %2").arg(root.memory.swapUsedText || "--").arg(root.memory.swapTotalText || "--"); color: "#e2e8f0" }
            }

            SectionTable {
                title: qsTr("Network Interfaces")
                headers: [qsTr("Name"), qsTr("Received"), qsTr("Sent")]
                columnWeights: [1.2, 1, 1]
                numericColumns: [1, 2]
                rows: root.networks.map(function(n) { return [n.name, n.rx, n.tx] })
            }

            SectionTable {
                title: qsTr("File Systems")
                headers: [qsTr("Name"), qsTr("Size"), qsTr("Used"), qsTr("Available"), qsTr("Use"), qsTr("Mount")]
                columnWeights: [1.5, 0.8, 0.8, 0.9, 0.55, 1.4]
                numericColumns: [1, 2, 3, 4]
                rows: root.filesystems.map(function(fs) {
                    return [fs.name, fs.size, fs.used, fs.available, root.pct(fs.usedPercent), fs.mount]
                })
            }

            SectionTable {
                title: qsTr("Top Processes")
                headers: [qsTr("PID"), qsTr("Command"), qsTr("CPU"), qsTr("Memory"), qsTr("RSS")]
                columnWeights: [0.7, 1.9, 0.65, 0.75, 0.85]
                numericColumns: [0, 2, 3, 4]
                rows: root.processes.map(function(p) {
                    return [p.pid, p.name, root.pct(p.cpu), root.pct(p.memory), p.rss]
                })
            }
        }
    }

    component SectionTable: ColumnLayout {
        id: table

        property string title: ""
        property var headers: []
        property var rows: []
        property var columnWeights: []
        property var numericColumns: []

        function weightAt(index) {
            return columnWeights && index < columnWeights.length ? Number(columnWeights[index]) : 1
        }

        function isNumericColumn(index) {
            return numericColumns && numericColumns.indexOf(index) >= 0
        }

        Layout.fillWidth: true
        spacing: 8

        Label {
            text: parent.title
            color: "#f8fafc"
            font.bold: true
            font.pixelSize: 14
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(34, 28 + parent.rows.length * 26)
            color: "#0f172a"
            border.color: "#1e293b"
            radius: 4

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Repeater {
                        model: table.headers.length
                        Label {
                            required property int index
                            text: table.headers[index]
                            color: "#60a5fa"
                            font.bold: true
                            font.pixelSize: 12
                            horizontalAlignment: table.isNumericColumn(index) ? Text.AlignRight : Text.AlignLeft
                            Layout.fillWidth: true
                            Layout.preferredWidth: table.weightAt(index) * 100
                            elide: Text.ElideRight
                        }
                    }
                }

                Repeater {
                    model: rows
                    RowLayout {
                        id: rowDelegate
                        required property var modelData

                        Layout.fillWidth: true
                        spacing: 12
                        Repeater {
                            model: rowDelegate.modelData.length
                            Label {
                                required property int index
                                text: String(rowDelegate.modelData[index] || "--")
                                color: "#cbd5e1"
                                font.pixelSize: 12
                                horizontalAlignment: table.isNumericColumn(index) ? Text.AlignRight : Text.AlignLeft
                                Layout.fillWidth: true
                                Layout.preferredWidth: table.weightAt(index) * 100
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }
    }
}
