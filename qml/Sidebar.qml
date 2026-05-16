import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var connections: []
    property string selectedConnectionId: ""

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
    }
}
