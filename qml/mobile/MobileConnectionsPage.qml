import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var connections: []
    property string statusMessage: ""

    signal refreshRequested()
    signal newConnectionRequested()
    signal connectionEditRequested(string id)
    signal connectionDeleteRequested(string id)
    signal connectionOpenRequested(string id)

    color: "#0b1220"

    function titleFor(connection) {
        if (connection.name && connection.name.length > 0) {
            return connection.name
        }
        if (connection.username && connection.username.length > 0) {
            return connection.username + "@" + (connection.host || "")
        }
        return connection.host || qsTr("(unnamed)")
    }

    function subtitleFor(connection) {
        const protocol = (connection.protocol || "ssh").toUpperCase()
        const port = String(connection.port || 22)
        const user = connection.username && connection.username.length > 0 ? connection.username + "@" : ""
        return protocol + "  " + user + (connection.host || "--") + ":" + port
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Connections")
                    color: "#f8fafc"
                    font.pixelSize: 22
                    font.bold: true
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("%1 saved").arg(root.connections.length)
                    color: "#94a3b8"
                    font.pixelSize: 12
                }
            }

            Button {
                Layout.preferredWidth: 48
                Layout.preferredHeight: 38
                text: "+"
                onClicked: root.newConnectionRequested()
            }

            Button {
                Layout.preferredWidth: 72
                Layout.preferredHeight: 38
                text: qsTr("Refresh")
                onClicked: root.refreshRequested()
            }
        }

        Label {
            Layout.fillWidth: true
            visible: root.statusMessage.length > 0
            text: root.statusMessage
            color: "#93c5fd"
            font.pixelSize: 12
            elide: Text.ElideRight
        }

        ListView {
            id: connectionList

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            model: root.connections

            delegate: Rectangle {
                id: row

                required property var modelData

                width: ListView.view.width
                height: 92
                radius: 8
                color: rowMouseArea.pressed ? "#1e3a8a" : "#111827"
                border.color: rowMouseArea.containsMouse ? "#38bdf8" : "#1e293b"

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12

                    Rectangle {
                        Layout.preferredWidth: 42
                        Layout.preferredHeight: 42
                        radius: 8
                        color: "#0f172a"
                        border.color: "#38bdf8"

                        Label {
                            anchors.centerIn: parent
                            text: (row.modelData.protocol || "ssh").substring(0, 1).toUpperCase()
                            color: "#e0f2fe"
                            font.pixelSize: 18
                            font.bold: true
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            Layout.fillWidth: true
                            text: root.titleFor(row.modelData)
                            color: "#f8fafc"
                            font.pixelSize: 15
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.subtitleFor(row.modelData)
                            color: "#94a3b8"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                    }

                    ColumnLayout {
                        spacing: 4

                        Button {
                            Layout.preferredWidth: 58
                            Layout.preferredHeight: 30
                            text: qsTr("Edit")
                            onClicked: root.connectionEditRequested(row.modelData.id)
                        }

                        Button {
                            Layout.preferredWidth: 58
                            Layout.preferredHeight: 30
                            text: qsTr("Delete")
                            onClicked: root.connectionDeleteRequested(row.modelData.id)
                        }
                    }
                }

                MouseArea {
                    id: rowMouseArea

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.connectionOpenRequested(row.modelData.id)
                }
            }

            Label {
                anchors.centerIn: parent
                visible: connectionList.count === 0
                text: qsTr("No connections yet")
                color: "#64748b"
                font.pixelSize: 14
            }
        }
    }
}
