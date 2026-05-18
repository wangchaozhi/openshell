import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    property var connections: []
    property string editingId: ""
    property string uiTheme: "dark"
    property string validationMessage: ""
    property bool validationShown: false

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
        validationMessage = ""
        validationShown = false
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
                validationMessage = ""
                validationShown = false
                open()
                return
            }
        }
    }

    function submit() {
        validationMessage = ""
        validationShown = true
        if (nameField.text.trim().length === 0) {
            nameField.forceActiveFocus()
            return
        }
        if (hostField.text.trim().length === 0) {
            hostField.forceActiveFocus()
            return
        }

        const payload = {
            "id": editingId,
            "name": nameField.text.trim(),
            "protocol": protocolBox.currentText,
            "host": hostField.text.trim(),
            "port": portField.value,
            "username": userField.text.trim(),
            "authType": authBox.currentText,
            "password": passwordField.text,
            "privateKeyPath": keyPathField.text,
            "group": groupField.text.trim(),
            "notes": notesField.text
        }
        if (appController.saveConnectionProfile(payload)) {
            root.saved()
            root.close()
        } else {
            validationMessage = appController.lastError
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
        height: 56
        color: root.classic ? "#f8fafc" : "#0f172a"
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
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 16
            spacing: 8

            ThemedButton {
                classic: root.classic
                text: qsTr("Cancel")
                onClicked: root.reject()
            }

            ThemedButton {
                classic: root.classic
                primary: true
                text: qsTr("OK")
                onClicked: root.submit()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 440
            clip: true
            contentWidth: availableWidth

        GridLayout {
            width: parent.width
            columns: 2
            columnSpacing: 12
            rowSpacing: 8

            Label { text: qsTr("Name"); color: root.classic ? "#334155" : "#94a3b8"; font.pixelSize: 12 }
            ThemedTextField {
                id: nameField
                classic: root.classic
                error: root.validationShown && text.trim().length === 0
                Layout.fillWidth: true
                placeholderText: error ? qsTr("Connection name is required") : ""
                onTextChanged: if (nameField.text.trim().length > 0 && hostField.text.trim().length > 0) root.validationShown = false
            }

            Label { text: qsTr("Protocol"); color: root.classic ? "#334155" : "#94a3b8"; font.pixelSize: 12 }
            ThemedComboBox {
                id: protocolBox
                classic: root.classic
                Layout.fillWidth: true
                model: ["ssh", "sftp", "telnet"]
            }

            Label { text: qsTr("Host"); color: root.classic ? "#334155" : "#94a3b8"; font.pixelSize: 12 }
            ThemedTextField {
                id: hostField
                classic: root.classic
                error: root.validationShown && text.trim().length === 0
                Layout.fillWidth: true
                placeholderText: error ? qsTr("Host is required") : "example.com"
                onTextChanged: if (nameField.text.trim().length > 0 && hostField.text.trim().length > 0) root.validationShown = false
            }

            Label { text: qsTr("Port"); color: root.classic ? "#334155" : "#94a3b8"; font.pixelSize: 12 }
            ThemedSpinBox {
                id: portField
                classic: root.classic
                from: 1
                to: 65535
                value: 22
                editable: true
                Layout.preferredWidth: 88
            }

            Label { text: qsTr("Username"); color: root.classic ? "#334155" : "#94a3b8"; font.pixelSize: 12 }
            ThemedTextField { id: userField; classic: root.classic; Layout.fillWidth: true }

            Label { text: qsTr("Auth Method"); color: root.classic ? "#334155" : "#94a3b8"; font.pixelSize: 12 }
            ThemedComboBox {
                id: authBox
                classic: root.classic
                Layout.fillWidth: true
                model: ["password", "key", "agent"]
            }

            Label { text: qsTr("Password"); color: root.classic ? "#334155" : "#94a3b8"; font.pixelSize: 12 }
            ThemedTextField {
                id: passwordField
                classic: root.classic
                Layout.fillWidth: true
                echoMode: TextInput.Password
                enabled: authBox.currentText === "password"
                opacity: enabled ? 1.0 : 0.4
            }

            Label { text: qsTr("Private Key"); color: root.classic ? "#334155" : "#94a3b8"; font.pixelSize: 12 }
            ThemedTextField {
                id: keyPathField
                classic: root.classic
                Layout.fillWidth: true
                placeholderText: qsTr("Absolute path to .pem / .ppk")
                enabled: authBox.currentText === "key"
                opacity: enabled ? 1.0 : 0.4
            }

            Label { text: qsTr("Group"); color: root.classic ? "#334155" : "#94a3b8"; font.pixelSize: 12 }
            ThemedTextField { id: groupField; classic: root.classic; Layout.fillWidth: true }

            Label {
                text: qsTr("Notes")
                color: root.classic ? "#334155" : "#94a3b8"
                font.pixelSize: 12
                Layout.alignment: Qt.AlignTop
            }
            ScrollView {
                id: notesScroll
                Layout.fillWidth: true
                Layout.preferredHeight: 72
                clip: true
                contentWidth: availableWidth
                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                ThemedTextArea {
                    id: notesField
                    classic: root.classic
                    width: notesScroll.availableWidth
                    height: Math.max(notesScroll.availableHeight, implicitHeight)
                }
            }
        }
        } // ScrollView

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 20
            radius: 3
            color: root.validationMessage.length > 0
                   ? (root.classic ? "#fee2e2" : "#450a0a")
                   : "transparent"

            Label {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                text: root.validationMessage
                color: root.classic ? "#b91c1c" : "#fca5a5"
                font.pixelSize: 12
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }
    }
}
