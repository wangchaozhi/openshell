import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var connections: []
    property string selectedConnectionId: ""
    property string uiTheme: "dark"
    readonly property bool classic: uiTheme === "classic"

    signal connectionPicked(string id)
    signal connectionOpenRequested(string id)
    signal newConnectionRequested()
    signal editConnectionRequested(string id)
    signal deleteConnectionRequested(string id)
    signal themeChanged(string theme)

    color: classic ? "#ffffff" : "#020617"

    function filteredConnections() {
        const q = searchField.text.trim().toLowerCase()
        const mode = filterBox.currentText
        return connections.filter(function(p) {
            const protocol = (p.protocol || "ssh").toUpperCase()
            if (mode !== qsTr("All") && protocol !== mode) {
                return false
            }
            if (q.length === 0) {
                return true
            }
            return (p.name || "").toLowerCase().indexOf(q) >= 0
                || (p.host || "").toLowerCase().indexOf(q) >= 0
                || (p.username || "").toLowerCase().indexOf(q) >= 0
                || (p.group || "").toLowerCase().indexOf(q) >= 0
        })
    }

    function groupedConnections() {
        const groups = []
        const indexByName = ({})
        const rows = filteredConnections()
        for (let i = 0; i < rows.length; ++i) {
            const row = rows[i]
            const name = (row.group && row.group.length > 0) ? row.group : qsTr("Connections")
            if (indexByName[name] === undefined) {
                indexByName[name] = groups.length
                groups.push({ name: name, rows: [] })
            }
            groups[indexByName[name]].rows.push(row)
        }
        groups.sort(function(a, b) { return a.name.localeCompare(b.name) })
        return groups
    }

    component PlusIcon: Item {
        implicitWidth: 14
        implicitHeight: 14
        Rectangle { anchors.centerIn: parent; width: 14; height: 2; radius: 1; color: root.classic ? "#0f172a" : "#dbeafe" }
        Rectangle { anchors.centerIn: parent; width: 2; height: 14; radius: 1; color: root.classic ? "#0f172a" : "#dbeafe" }
    }

    component FolderIcon: Item {
        implicitWidth: 16
        implicitHeight: 14
        Rectangle {
            x: 1
            y: 5
            width: 14
            height: 8
            radius: 1
            color: root.classic ? "#fde68a" : "#38bdf8"
            border.color: root.classic ? "#f59e0b" : "#93c5fd"
        }
        Rectangle {
            x: 2
            y: 2
            width: 7
            height: 5
            radius: 1
            color: root.classic ? "#fde68a" : "#38bdf8"
            border.color: root.classic ? "#f59e0b" : "#93c5fd"
        }
    }

    component HostIcon: Item {
        implicitWidth: 16
        implicitHeight: 16
        Rectangle {
            x: 2
            y: 3
            width: 12
            height: 9
            radius: 1
            color: root.classic ? "#475569" : "#38bdf8"
            border.color: root.classic ? "#0f172a" : "#93c5fd"
        }
        Rectangle {
            x: 5
            y: 12
            width: 6
            height: 2
            color: root.classic ? "#0f172a" : "#93c5fd"
        }
    }

    component ThemedTextField: TextField {
        color: root.classic ? "#0f172a" : "#e2e8f0"
        placeholderTextColor: root.classic ? "#94a3b8" : "#64748b"
        selectedTextColor: "#ffffff"
        selectionColor: "#2563eb"
        background: Rectangle {
            radius: 2
            color: root.classic ? "#ffffff" : "#0f172a"
            border.color: parent.activeFocus ? "#38bdf8" : (root.classic ? "#cbd5e1" : "#334155")
        }
    }

    component ThemedComboBox: ComboBox {
        id: combo

        property int popupMaxHeight: 220

        contentItem: Label {
            leftPadding: 8
            rightPadding: 22
            verticalAlignment: Text.AlignVCenter
            text: combo.displayText
            color: root.classic ? "#0f172a" : "#e2e8f0"
            elide: Text.ElideRight
            font.pixelSize: 12
        }
        indicator: Canvas {
            x: combo.width - width - 8
            y: (combo.height - height) / 2
            width: 8
            height: 5
            contextType: "2d"
            onPaint: {
                context.reset()
                context.moveTo(0, 0)
                context.lineTo(width, 0)
                context.lineTo(width / 2, height)
                context.closePath()
                context.fillStyle = root.classic ? "#475569" : "#94a3b8"
                context.fill()
            }
        }
        background: Rectangle {
            radius: 2
            color: root.classic ? "#ffffff" : "#0f172a"
            border.color: combo.activeFocus || combo.down ? "#38bdf8" : (root.classic ? "#cbd5e1" : "#334155")
        }
        delegate: ItemDelegate {
            width: combo.width
            height: 28
            highlighted: combo.highlightedIndex === index
            contentItem: Label {
                text: combo.textRole && modelData[combo.textRole] !== undefined ? modelData[combo.textRole] : modelData
                color: highlighted ? "#ffffff" : (root.classic ? "#0f172a" : "#e2e8f0")
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 12
            }
            background: Rectangle {
                color: highlighted ? "#2563eb" : (root.classic ? "#ffffff" : "#0f172a")
            }
        }
        popup: Popup {
            y: combo.height + 2
            width: combo.width
            implicitHeight: Math.min(contentItem.implicitHeight + 2, combo.popupMaxHeight)
            padding: 1
            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: combo.popup.visible ? combo.delegateModel : null
                currentIndex: combo.highlightedIndex
            }
            background: Rectangle {
                color: root.classic ? "#ffffff" : "#0f172a"
                border.color: root.classic ? "#cbd5e1" : "#334155"
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Label {
                text: qsTr("Connection Manager")
                color: root.classic ? "#334155" : "#e2e8f0"
                font.pixelSize: 13
                Layout.preferredWidth: 150
            }

            ToolButton {
                Layout.preferredWidth: 28
                Layout.preferredHeight: 26
                contentItem: PlusIcon { anchors.centerIn: parent }
                background: Rectangle {
                    radius: 2
                    color: parent.hovered ? (root.classic ? "#e0f2fe" : "#1e3a8a")
                                          : (root.classic ? "#f8fafc" : "#1e293b")
                    border.color: parent.hovered ? "#38bdf8" : (root.classic ? "#cbd5e1" : "#334155")
                }
                ToolTip.visible: hovered
                ToolTip.text: qsTr("New connection")
                onClicked: root.newConnectionRequested()
            }

            ThemedTextField {
                id: searchField
                Layout.preferredWidth: 260
                placeholderText: qsTr("Search")
            }

            ThemedComboBox {
                id: filterBox
                Layout.preferredWidth: 90
                model: [qsTr("All"), "SSH", "SFTP", "TELNET"]
            }

            Item { Layout.fillWidth: true }

            Label {
                text: qsTr("Theme")
                color: root.classic ? "#64748b" : "#94a3b8"
                font.pixelSize: 11
            }

            ThemedComboBox {
                id: themeBox
                Layout.preferredWidth: 118
                textRole: "label"
                valueRole: "value"
                model: [
                    { label: qsTr("Dark"), value: "dark" },
                    { label: qsTr("Classic"), value: "classic" }
                ]
                Component.onCompleted: currentIndex = root.uiTheme === "classic" ? 1 : 0
                onActivated: root.themeChanged(currentValue)
                Connections {
                    target: root
                    function onUiThemeChanged() {
                        themeBox.currentIndex = root.uiTheme === "classic" ? 1 : 0
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: root.classic ? "#cbd5e1" : "#1e293b"
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 16
            Label {
                text: qsTr("Name")
                color: root.classic ? "#64748b" : "#60a5fa"
                font.pixelSize: 11
                Layout.preferredWidth: 210
            }
            Label {
                text: qsTr("Host")
                color: root.classic ? "#64748b" : "#60a5fa"
                font.pixelSize: 11
                Layout.preferredWidth: 180
            }
            Label {
                text: qsTr("Port")
                color: root.classic ? "#64748b" : "#60a5fa"
                font.pixelSize: 11
                Layout.preferredWidth: 70
            }
            Label {
                text: qsTr("Username")
                color: root.classic ? "#64748b" : "#60a5fa"
                font.pixelSize: 11
                Layout.preferredWidth: 120
            }
            Item { Layout.fillWidth: true }
        }

        ScrollView {
            id: connectionScroll

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: connectionScroll.availableWidth
                spacing: 0

                Repeater {
                    model: root.groupedConnections()
                    delegate: ColumnLayout {
                        required property var modelData

                        Layout.fillWidth: true
                        Layout.preferredWidth: connectionScroll.availableWidth
                        spacing: 0

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredWidth: connectionScroll.availableWidth
                            Layout.preferredHeight: 28
                            spacing: 6

                            FolderIcon {}
                            Label {
                                text: modelData.name
                                color: root.classic ? "#0f172a" : "#f8fafc"
                                font.bold: true
                                font.pixelSize: 12
                                Layout.fillWidth: true
                            }
                            Label {
                                text: String(modelData.rows.length)
                                color: root.classic ? "#64748b" : "#94a3b8"
                                font.pixelSize: 11
                            }
                        }

                        Repeater {
                            model: modelData.rows
                            delegate: Rectangle {
                                id: connectionRow

                                required property var modelData
                                readonly property bool selected: modelData.id === root.selectedConnectionId
                                readonly property bool hovered: rowMouseArea.containsMouse

                                Layout.fillWidth: true
                                Layout.preferredWidth: connectionScroll.availableWidth
                                Layout.preferredHeight: 28
                                width: connectionScroll.availableWidth
                                color: selected
                                       ? (root.classic ? "#e0f2fe" : "#1e3a8a")
                                       : hovered
                                         ? (root.classic ? "#f1f5f9" : "#172033")
                                       : "transparent"

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 20
                                    anchors.rightMargin: 8
                                    spacing: 16

                                    RowLayout {
                                        Layout.preferredWidth: 190
                                        spacing: 6
                                        HostIcon {}
                                        Label {
                                            text: modelData.name || qsTr("(unnamed)")
                                            color: root.classic ? "#0f172a" : "#f8fafc"
                                            font.pixelSize: 12
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                    }

                                    Label {
                                        text: modelData.host || ""
                                        color: root.classic ? "#0f172a" : "#cbd5e1"
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                        Layout.preferredWidth: 180
                                    }
                                    Label {
                                        text: String(modelData.port || 22)
                                        color: root.classic ? "#0f172a" : "#cbd5e1"
                                        font.pixelSize: 12
                                        horizontalAlignment: Text.AlignRight
                                        Layout.preferredWidth: 70
                                    }
                                    Label {
                                        text: modelData.username || ""
                                        color: root.classic ? "#0f172a" : "#cbd5e1"
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                        Layout.preferredWidth: 120
                                    }
                                    Item { Layout.fillWidth: true }
                                }

                                MouseArea {
                                    id: rowMouseArea

                                    anchors.fill: parent
                                    z: 20
                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: function(mouse) {
                                        root.connectionPicked(modelData.id)
                                        if (mouse.button === Qt.RightButton) {
                                            contextMenu.popup()
                                        } else {
                                            root.connectionOpenRequested(modelData.id)
                                        }
                                    }

                                    Menu {
                                        id: contextMenu
                                        MenuItem {
                                            text: qsTr("Open Session")
                                            onTriggered: root.connectionOpenRequested(modelData.id)
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
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.connections.length === 0
                    text: qsTr("No connections yet. Press + to add one.")
                    color: "#94a3b8"
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: 12
                    padding: 28
                }
            }
        }
    }
}
