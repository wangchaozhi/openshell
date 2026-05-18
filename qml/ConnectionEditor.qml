import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    property var connections: []
    property string editingId: ""
    property string uiTheme: "dark"

    readonly property bool classic: uiTheme === "classic"

    signal saved()
    signal saveFailed(string message)

    title: editingId.length === 0 ? qsTr("New Connection") : qsTr("Edit Connection")
    width: 480
    padding: 0

    function openForNew() {
        editingId = ""
        nameField.text = ""
        hostField.text = ""
        portField.value = 22
        userField.text = ""
        passwordField.text = ""
        keyPathField.text = ""
        protocolBox.currentIndex = 0
        authBox.currentIndex = 0
        groupField.text = ""
        notesField.text = ""
        open()
    }

    function openForEdit(id) {
        for (let i = 0; i < connections.length; ++i) {
            if (connections[i].id === id) {
                const p = connections[i]
                editingId = p.id
                nameField.text = p.name || ""
                hostField.text = p.host || ""
                portField.value = p.port || 22
                userField.text = p.username || ""
                passwordField.text = p.password || ""
                keyPathField.text = p.privateKeyPath || ""
                protocolBox.currentIndex = Math.max(0, ["ssh", "sftp", "telnet"].indexOf(p.protocol || "ssh"))
                authBox.currentIndex = Math.max(0, ["password", "key", "agent"].indexOf(p.authType || "password"))
                groupField.text = p.group || ""
                notesField.text = p.notes || ""
                open()
                return
            }
        }
    }

    onAccepted: {
        const payload = {
            "id": editingId,
            "name": nameField.text,
            "protocol": protocolBox.currentText,
            "host": hostField.text,
            "port": portField.value,
            "username": userField.text,
            "authType": authBox.currentText,
            "password": passwordField.text,
            "privateKeyPath": keyPathField.text,
            "group": groupField.text,
            "notes": notesField.text
        }
        if (appController.saveConnectionProfile(payload)) {
            root.saved()
        } else {
            root.saveFailed(appController.lastError)
        }
    }

    background: Rectangle {
        color: root.classic ? "#ffffff" : "#1e293b"
        border.color: root.classic ? "#cbd5e1" : "#334155"
        radius: 6
    }

    header: Rectangle {
        color: root.classic ? "#f8fafc" : "#0f172a"
        height: 44
        radius: 6

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 6
            color: parent.color
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: root.classic ? "#e2e8f0" : "#1e293b"
        }

        Label {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 16
            text: root.title
            color: root.classic ? "#0f172a" : "#f8fafc"
            font.pixelSize: 14
            font.bold: true
        }
    }

    footer: Rectangle {
        color: root.classic ? "#f8fafc" : "#0f172a"
        height: 52
        radius: 6

        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 6
            color: parent.color
        }

        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 1
            color: root.classic ? "#e2e8f0" : "#1e293b"
        }

        RowLayout {
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            Button {
                text: qsTr("Cancel")
                onClicked: root.reject()
                background: Rectangle {
                    radius: 4
                    color: parent.hovered ? (root.classic ? "#f1f5f9" : "#1e3a5f")
                                          : (root.classic ? "#ffffff" : "#1e293b")
                    border.color: root.classic ? "#cbd5e1" : "#475569"
                }
                contentItem: Label {
                    text: parent.text
                    color: root.classic ? "#334155" : "#cbd5e1"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 13
                }
                implicitWidth: 80
                implicitHeight: 32
            }

            Button {
                text: qsTr("OK")
                onClicked: root.accept()
                background: Rectangle {
                    radius: 4
                    color: parent.hovered ? "#1d4ed8" : "#2563eb"
                }
                contentItem: Label {
                    text: parent.text
                    color: "#ffffff"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 13
                }
                implicitWidth: 80
                implicitHeight: 32
            }
        }
    }

    component ThemedTextField: TextField {
        color: root.classic ? "#0f172a" : "#e2e8f0"
        placeholderTextColor: root.classic ? "#94a3b8" : "#64748b"
        selectedTextColor: "#ffffff"
        selectionColor: "#2563eb"
        background: Rectangle {
            radius: 3
            color: root.classic ? "#ffffff" : "#0f172a"
            border.color: parent.activeFocus ? "#38bdf8" : (root.classic ? "#cbd5e1" : "#334155")
        }
    }

    component ThemedComboBox: ComboBox {
        id: combo
        contentItem: Label {
            leftPadding: 8
            rightPadding: 22
            verticalAlignment: Text.AlignVCenter
            text: combo.displayText
            color: root.classic ? "#0f172a" : "#e2e8f0"
            elide: Text.ElideRight
            font.pixelSize: 13
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
            radius: 3
            color: root.classic ? "#ffffff" : "#0f172a"
            border.color: combo.activeFocus || combo.down ? "#38bdf8" : (root.classic ? "#cbd5e1" : "#334155")
        }
        delegate: ItemDelegate {
            width: combo.width
            height: 28
            highlighted: combo.highlightedIndex === index
            contentItem: Label {
                text: modelData
                color: highlighted ? "#ffffff" : (root.classic ? "#0f172a" : "#e2e8f0")
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 13
            }
            background: Rectangle {
                color: highlighted ? "#2563eb" : (root.classic ? "#ffffff" : "#0f172a")
            }
        }
        popup: Popup {
            y: combo.height + 2
            width: combo.width
            implicitHeight: Math.min(contentItem.implicitHeight + 2, 160)
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
                radius: 3
            }
        }
    }

    component ThemedSpinBox: SpinBox {
        id: spin
        contentItem: TextInput {
            z: 2
            text: spin.textFromValue(spin.value, spin.locale)
            color: root.classic ? "#0f172a" : "#e2e8f0"
            selectionColor: "#2563eb"
            selectedTextColor: "#ffffff"
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
            readOnly: !spin.editable
            validator: spin.validator
            inputMethodHints: Qt.ImhFormattedNumbersOnly
        }
        up.indicator: Rectangle {
            x: spin.mirrored ? 0 : parent.width - width
            height: parent.height
            implicitWidth: 28
            implicitHeight: 36
            color: spin.up.hovered ? (root.classic ? "#e0f2fe" : "#1e3a8a") : "transparent"
            border.color: root.classic ? "#cbd5e1" : "#334155"
            border.width: 0
            Text {
                text: "+"
                anchors.centerIn: parent
                color: root.classic ? "#334155" : "#94a3b8"
                font.pixelSize: 14
            }
        }
        down.indicator: Rectangle {
            x: spin.mirrored ? parent.width - width : 0
            height: parent.height
            implicitWidth: 28
            implicitHeight: 36
            color: spin.down.hovered ? (root.classic ? "#e0f2fe" : "#1e3a8a") : "transparent"
            border.color: root.classic ? "#cbd5e1" : "#334155"
            border.width: 0
            Text {
                text: "−"
                anchors.centerIn: parent
                color: root.classic ? "#334155" : "#94a3b8"
                font.pixelSize: 14
            }
        }
        background: Rectangle {
            radius: 3
            color: root.classic ? "#ffffff" : "#0f172a"
            border.color: spin.activeFocus ? "#38bdf8" : (root.classic ? "#cbd5e1" : "#334155")
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 12
            rowSpacing: 8

            Label { text: qsTr("Name"); color: root.classic ? "#334155" : "#94a3b8"; font.pixelSize: 12 }
            ThemedTextField { id: nameField; Layout.fillWidth: true }

            Label { text: qsTr("Protocol"); color: root.classic ? "#334155" : "#94a3b8"; font.pixelSize: 12 }
            ThemedComboBox {
                id: protocolBox
                Layout.fillWidth: true
                model: ["ssh", "sftp", "telnet"]
            }

            Label { text: qsTr("Host"); color: root.classic ? "#334155" : "#94a3b8"; font.pixelSize: 12 }
            ThemedTextField { id: hostField; Layout.fillWidth: true; placeholderText: "example.com" }

            Label { text: qsTr("Port"); color: root.classic ? "#334155" : "#94a3b8"; font.pixelSize: 12 }
            ThemedSpinBox { id: portField; Layout.fillWidth: true; from: 1; to: 65535; value: 22 }

            Label { text: qsTr("Username"); color: root.classic ? "#334155" : "#94a3b8"; font.pixelSize: 12 }
            ThemedTextField { id: userField; Layout.fillWidth: true }

            Label { text: qsTr("Auth Method"); color: root.classic ? "#334155" : "#94a3b8"; font.pixelSize: 12 }
            ThemedComboBox {
                id: authBox
                Layout.fillWidth: true
                model: ["password", "key", "agent"]
            }

            Label { text: qsTr("Password"); color: root.classic ? "#334155" : "#94a3b8"; font.pixelSize: 12 }
            ThemedTextField {
                id: passwordField
                Layout.fillWidth: true
                echoMode: TextInput.Password
                enabled: authBox.currentText === "password"
                opacity: enabled ? 1.0 : 0.4
            }

            Label { text: qsTr("Private Key"); color: root.classic ? "#334155" : "#94a3b8"; font.pixelSize: 12 }
            ThemedTextField {
                id: keyPathField
                Layout.fillWidth: true
                placeholderText: qsTr("Absolute path to .pem / .ppk")
                enabled: authBox.currentText === "key"
                opacity: enabled ? 1.0 : 0.4
            }

            Label { text: qsTr("Group"); color: root.classic ? "#334155" : "#94a3b8"; font.pixelSize: 12 }
            ThemedTextField { id: groupField; Layout.fillWidth: true }

            Label { text: qsTr("Notes"); color: root.classic ? "#334155" : "#94a3b8"; font.pixelSize: 12 }
            TextArea {
                id: notesField
                Layout.fillWidth: true
                Layout.preferredHeight: 72
                wrapMode: TextArea.Wrap
                color: root.classic ? "#0f172a" : "#e2e8f0"
                selectedTextColor: "#ffffff"
                selectionColor: "#2563eb"
                background: Rectangle {
                    radius: 3
                    color: root.classic ? "#ffffff" : "#0f172a"
                    border.color: notesField.activeFocus ? "#38bdf8" : (root.classic ? "#cbd5e1" : "#334155")
                }
            }
        }
    }
}
