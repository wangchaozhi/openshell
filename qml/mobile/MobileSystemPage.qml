import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var snapshot: ({})
    property string error: ""
    property bool hasSession: false

    color: "#0b1220"

    readonly property var cpu: snapshot.cpu || ({})
    readonly property var memory: snapshot.memory || ({})
    readonly property var info: snapshot.info || ({})

    function pct(value) {
        return (Number(value || 0)).toFixed(1) + "%"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Label {
            Layout.fillWidth: true
            text: qsTr("System")
            color: "#f8fafc"
            font.pixelSize: 22
            font.bold: true
        }

        Label {
            Layout.fillWidth: true
            text: root.error.length > 0 ? root.error
                  : (root.hasSession ? qsTr("Waiting for monitor data") : qsTr("Open a session first"))
            color: root.error.length > 0 ? "#fca5a5" : "#94a3b8"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        Repeater {
            model: [
                { label: qsTr("Host"), value: root.info.hostname || "--" },
                { label: qsTr("CPU"), value: root.pct(root.cpu.busyPercent) },
                { label: qsTr("Memory"), value: qsTr("%1 / %2").arg(root.memory.memUsedText || "--").arg(root.memory.memTotalText || "--") },
                { label: qsTr("Swap"), value: qsTr("%1 / %2").arg(root.memory.swapUsedText || "--").arg(root.memory.swapTotalText || "--") }
            ]

            delegate: Rectangle {
                required property var modelData

                Layout.fillWidth: true
                Layout.preferredHeight: 64
                radius: 8
                color: "#111827"
                border.color: "#1e293b"

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 14

                    Label {
                        text: modelData.label
                        color: "#93c5fd"
                        font.pixelSize: 13
                        Layout.fillWidth: true
                    }

                    Label {
                        text: modelData.value
                        color: "#e2e8f0"
                        font.pixelSize: 14
                        font.bold: true
                    }
                }
            }
        }
    }
}
