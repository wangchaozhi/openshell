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

            // 不固定宽度，让 Material 按钮按文字内容自适应——之前的 48 / 72
            // 比 Material Filled 按钮的左右 padding 合计还小，会把文字 elide 掉。
            Button {
                Layout.preferredHeight: 40
                text: qsTr("New")
                onClicked: root.newConnectionRequested()
            }

            Button {
                Layout.preferredHeight: 40
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

        Label {
            Layout.fillWidth: true
            visible: root.connections.length > 0
            text: qsTr("Tap to connect, long-press for actions")
            color: "#64748b"
            font.pixelSize: 11
        }

        Menu {
            id: rowMenu

            property string targetId: ""
            property string targetTitle: ""

            // Material 默认 Menu 会按字体 + padding 给一个偏大的 implicitWidth，
            // 短词 (Connect / Edit / Delete) 会留一大块右侧空白。这里收紧到
            // 内容宽度并贴右侧对齐。
            width: 100

            // Material 默认 MenuItem 高度 ~48dp，对手指够大但视觉偏臃肿，
            // 把每条压到 36 让整个菜单更紧凑。
            MenuItem {
                width: parent.width
                height: 36
                text: qsTr("Connect")
                enabled: rowMenu.targetId.length > 0
                onTriggered: root.connectionOpenRequested(rowMenu.targetId)
            }
            MenuSeparator {}
            MenuItem {
                width: parent.width
                height: 36
                text: qsTr("Edit")
                enabled: rowMenu.targetId.length > 0
                onTriggered: root.connectionEditRequested(rowMenu.targetId)
            }
            MenuItem {
                width: parent.width
                height: 36
                text: qsTr("Delete")
                enabled: rowMenu.targetId.length > 0
                onTriggered: root.connectionDeleteRequested(rowMenu.targetId)
            }
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
                height: 80
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
                }

                // 整行：点 = 打开连接；长按 = 弹 Edit/Delete 菜单。
                // 没有按钮夹在中间，触摸事件不会撕扯，也省出了完整宽度给
                // 主机名展示。
                MouseArea {
                    id: rowMouseArea

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.connectionOpenRequested(row.modelData.id)
                    onPressAndHold: (mouse) => {
                        rowMenu.targetId = row.modelData.id
                        rowMenu.targetTitle = root.titleFor(row.modelData)
                        // 菜单宽度由 rowMenu.width 控制（见下面 Menu 定义），
                        // 这里按这个宽度让菜单右边贴齐行的右边距 8。
                        const x = Math.max(0, row.width - rowMenu.width - 8)
                        rowMenu.popup(row, x, mouse.y)
                    }
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
