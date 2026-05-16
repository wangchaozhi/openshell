import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    property var connections: []
    property string editingId: ""

    signal saved()
    signal saveFailed(string message)

    title: editingId.length === 0 ? qsTr("New Connection") : qsTr("Edit Connection")
    width: 480
    standardButtons: Dialog.Ok | Dialog.Cancel

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

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 12
            rowSpacing: 8

            Label { text: qsTr("Name") }
            TextField { id: nameField; Layout.fillWidth: true }

            Label { text: qsTr("Protocol") }
            ComboBox {
                id: protocolBox
                Layout.fillWidth: true
                model: ["ssh", "sftp", "telnet"]
            }

            Label { text: qsTr("Host") }
            TextField { id: hostField; Layout.fillWidth: true; placeholderText: "example.com" }

            Label { text: qsTr("Port") }
            SpinBox { id: portField; Layout.fillWidth: true; from: 1; to: 65535; value: 22 }

            Label { text: qsTr("Username") }
            TextField { id: userField; Layout.fillWidth: true }

            Label { text: qsTr("Auth Method") }
            ComboBox {
                id: authBox
                Layout.fillWidth: true
                model: ["password", "key", "agent"]
            }

            Label { text: qsTr("Password") }
            TextField {
                id: passwordField
                Layout.fillWidth: true
                echoMode: TextInput.Password
                enabled: authBox.currentText === "password"
            }

            Label { text: qsTr("Private Key") }
            TextField {
                id: keyPathField
                Layout.fillWidth: true
                placeholderText: qsTr("Absolute path to .pem / .ppk")
                enabled: authBox.currentText === "key"
            }

            Label { text: qsTr("Group") }
            TextField { id: groupField; Layout.fillWidth: true }

            Label { text: qsTr("Notes") }
            TextArea {
                id: notesField
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                wrapMode: TextArea.Wrap
            }
        }
    }
}
