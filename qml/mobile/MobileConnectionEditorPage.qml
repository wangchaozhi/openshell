import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property string editingId: ""
    property string errorMessage: ""

    signal saved()
    signal canceled()

    color: "#0b1220"

    function openForNew() {
        editingId = ""
        nameField.text = ""
        protocolBox.currentIndex = 0
        hostField.text = ""
        portField.text = "22"
        userField.text = ""
        authBox.currentIndex = 0
        passwordField.text = ""
        keyPathField.text = ""
        groupField.text = ""
        notesField.text = ""
        errorMessage = ""
    }

    function openForEdit(profile) {
        editingId = profile.id || ""
        nameField.text = profile.name || ""
        protocolBox.currentIndex = Math.max(0, ["ssh", "sftp"].indexOf(profile.protocol || "ssh"))
        hostField.text = profile.host || ""
        portField.text = String(profile.port || 22)
        userField.text = profile.username || ""
        authBox.currentIndex = Math.max(0, ["password", "key", "agent"].indexOf(profile.authType || "password"))
        passwordField.text = profile.password || ""
        keyPathField.text = profile.privateKeyPath || ""
        groupField.text = profile.group || ""
        notesField.text = profile.notes || ""
        errorMessage = ""
    }

    function save() {
        errorMessage = ""
        if (nameField.text.trim().length === 0) {
            errorMessage = qsTr("Connection name is required")
            nameField.forceActiveFocus()
            return
        }
        if (hostField.text.trim().length === 0) {
            errorMessage = qsTr("Host is required")
            hostField.forceActiveFocus()
            return
        }

        const payload = {
            "id": editingId,
            "name": nameField.text.trim(),
            "protocol": protocolBox.currentText,
            "host": hostField.text.trim(),
            "port": Math.max(1, Math.min(65535, Number(portField.text) || 22)),
            "username": userField.text.trim(),
            "authType": authBox.currentText,
            "password": passwordField.text,
            "privateKeyPath": keyPathField.text,
            "group": groupField.text.trim(),
            "notes": notesField.text
        }

        if (appController.saveConnectionProfile(payload)) {
            saved()
        } else {
            errorMessage = appController.lastError
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            color: "#0f172a"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                Button {
                    Layout.preferredHeight: 38
                    text: qsTr("Back")
                    onClicked: root.canceled()
                }

                Label {
                    Layout.fillWidth: true
                    text: root.editingId.length === 0 ? qsTr("New Connection") : qsTr("Edit Connection")
                    color: "#f8fafc"
                    font.pixelSize: 17
                    font.bold: true
                    elide: Text.ElideRight
                }

                Button {
                    Layout.preferredHeight: 38
                    text: qsTr("Save")
                    onClicked: root.save()
                }
            }
        }

        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: formColumn.implicitHeight + 24
            clip: true

            ColumnLayout {
                id: formColumn

                width: parent.width
                spacing: 12

                Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    Layout.topMargin: 16
                    visible: root.errorMessage.length > 0
                    text: root.errorMessage
                    color: "#fca5a5"
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                Label { Layout.leftMargin: 16; text: qsTr("Name"); color: "#93c5fd"; font.pixelSize: 12 }
                TextField { id: nameField; Layout.fillWidth: true; Layout.leftMargin: 16; Layout.rightMargin: 16 }

                Label { Layout.leftMargin: 16; text: qsTr("Protocol"); color: "#93c5fd"; font.pixelSize: 12 }
                ComboBox {
                    id: protocolBox
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    model: ["ssh", "sftp"]
                }

                Label { Layout.leftMargin: 16; text: qsTr("Host"); color: "#93c5fd"; font.pixelSize: 12 }
                TextField {
                    id: hostField
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    placeholderText: qsTr("Hostname or IP")
                }

                Label { Layout.leftMargin: 16; text: qsTr("Port"); color: "#93c5fd"; font.pixelSize: 12 }
                TextField {
                    id: portField
                    Layout.preferredWidth: 120
                    Layout.leftMargin: 16
                    horizontalAlignment: TextInput.AlignHCenter
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator { bottom: 1; top: 65535 }
                    onEditingFinished: text = String(Math.max(1, Math.min(65535, Number(text) || 22)))
                }

                Label { Layout.leftMargin: 16; text: qsTr("Username"); color: "#93c5fd"; font.pixelSize: 12 }
                TextField { id: userField; Layout.fillWidth: true; Layout.leftMargin: 16; Layout.rightMargin: 16 }

                Label { Layout.leftMargin: 16; text: qsTr("Auth Method"); color: "#93c5fd"; font.pixelSize: 12 }
                ComboBox {
                    id: authBox
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    model: ["password", "key", "agent"]
                }

                Label { Layout.leftMargin: 16; text: qsTr("Password"); color: "#93c5fd"; font.pixelSize: 12 }
                TextField {
                    id: passwordField
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    echoMode: TextInput.Password
                    enabled: authBox.currentText === "password"
                    opacity: enabled ? 1.0 : 0.45
                }

                Label { Layout.leftMargin: 16; text: qsTr("Private Key"); color: "#93c5fd"; font.pixelSize: 12 }
                TextField {
                    id: keyPathField
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    placeholderText: qsTr("Absolute path to .pem / .ppk")
                    enabled: authBox.currentText === "key"
                    opacity: enabled ? 1.0 : 0.45
                }

                Label { Layout.leftMargin: 16; text: qsTr("Group"); color: "#93c5fd"; font.pixelSize: 12 }
                TextField { id: groupField; Layout.fillWidth: true; Layout.leftMargin: 16; Layout.rightMargin: 16 }

                Label { Layout.leftMargin: 16; text: qsTr("Notes"); color: "#93c5fd"; font.pixelSize: 12 }
                TextArea {
                    id: notesField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 96
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    wrapMode: TextArea.Wrap
                }
            }
        }
    }
}
