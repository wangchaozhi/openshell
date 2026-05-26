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

    readonly property bool classic: theme.classic
    readonly property var protocols: ["ssh", "sftp", "telnet"]
    readonly property bool isTelnet: protocolBox.currentText === "telnet"

    ThemePalette {
        id: theme
        mode: root.uiTheme
    }

    signal saved()
    signal saveFailed(string message)

    title: editingId.length === 0 ? qsTr("New Connection") : qsTr("Edit Connection")
    width: 520
    height: 640
    padding: 0

    // Serialise the forwards list (from C++) into the editor's textarea format.
    function _forwardsToText(arr) {
        if (!arr || !arr.length) return ""
        const lines = []
        for (let i = 0; i < arr.length; ++i) {
            const f = arr[i]
            const bind = (f.bindHost || "127.0.0.1") + ":" + (f.bindPort || 0)
            const remote = (f.remoteHost || "") + ":" + (f.remotePort || 0)
            lines.push(bind + " -> " + remote)
        }
        return lines.join("\n")
    }

    // Parse the textarea into a list of forwards. Ignores blank lines / comments.
    function _parseForwards(text) {
        const result = []
        if (!text) return result
        const lines = text.split(/\r?\n/)
        for (let i = 0; i < lines.length; ++i) {
            let line = lines[i].trim()
            if (!line || line.charAt(0) === "#") continue
            const parts = line.split("->")
            if (parts.length !== 2) continue
            const left = parts[0].trim().split(":")
            const right = parts[1].trim().split(":")
            if (left.length < 2 || right.length < 2) continue
            const bindPort = parseInt(left[left.length - 1], 10)
            const remotePort = parseInt(right[right.length - 1], 10)
            const bindHost = left.slice(0, left.length - 1).join(":") || "127.0.0.1"
            const remoteHost = right.slice(0, right.length - 1).join(":")
            if (!bindPort || !remotePort || !remoteHost) continue
            result.push({
                "type": "L",
                "bindHost": bindHost,
                "bindPort": bindPort,
                "remoteHost": remoteHost,
                "remotePort": remotePort
            })
        }
        return result
    }

    function openForNew() {
        editingId = ""
        nameField.text = ""
        hostField.text = ""
        portField.text = "22"
        userField.text = ""
        passwordField.text = ""
        keyPathField.text = ""
        protocolBox.currentIndex = 0
        authBox.currentIndex = 0
        groupField.text = ""
        notesField.text = ""
        autoReconnectCheck.checked = true
        telnetAutoLoginCheck.checked = true
        telnetTerminalTypeField.text = "xterm-256color"
        jumpHostField.text = ""
        jumpPortField.text = "22"
        jumpUserField.text = ""
        jumpPasswordField.text = ""
        jumpKeyPathField.text = ""
        jumpAuthBox.currentIndex = 0
        forwardsField.text = ""
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
                portField.text = String(p.port || 22)
                userField.text = p.username || ""
                passwordField.text = p.password || ""
                keyPathField.text = p.privateKeyPath || ""
                protocolBox.currentIndex = Math.max(0, root.protocols.indexOf(p.protocol || "ssh"))
                authBox.currentIndex = Math.max(0, ["password", "key", "agent"].indexOf(p.authType || "password"))
                groupField.text = p.group || ""
                notesField.text = p.notes || ""
                autoReconnectCheck.checked = (p.autoReconnect !== false)
                telnetAutoLoginCheck.checked = (p.telnetAutoLogin !== false)
                telnetTerminalTypeField.text = p.telnetTerminalType || "xterm-256color"
                jumpHostField.text = p.jumpHost || ""
                jumpPortField.text = String(p.jumpPort || 22)
                jumpUserField.text = p.jumpUsername || ""
                jumpPasswordField.text = p.jumpPassword || ""
                jumpKeyPathField.text = p.jumpPrivateKeyPath || ""
                jumpAuthBox.currentIndex = Math.max(0, ["password", "key", "agent"].indexOf(p.jumpAuthType || "password"))
                forwardsField.text = _forwardsToText(p.forwards)
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
            "port": Math.max(1, Math.min(65535, Number(portField.text) || 22)),
            "username": userField.text.trim(),
            "authType": root.isTelnet ? "password" : authBox.currentText,
            "password": passwordField.text,
            "privateKeyPath": root.isTelnet ? "" : keyPathField.text,
            "group": groupField.text.trim(),
            "notes": notesField.text,
            "autoReconnect": autoReconnectCheck.checked,
            "telnetAutoLogin": telnetAutoLoginCheck.checked,
            "telnetTerminalType": telnetTerminalTypeField.text.trim() || "xterm-256color",
            "jumpHost": root.isTelnet ? "" : jumpHostField.text.trim(),
            "jumpPort": Math.max(1, Math.min(65535, Number(jumpPortField.text) || 22)),
            "jumpUsername": root.isTelnet ? "" : jumpUserField.text.trim(),
            "jumpAuthType": root.isTelnet ? "" : jumpAuthBox.currentText,
            "jumpPassword": root.isTelnet ? "" : jumpPasswordField.text,
            "jumpPrivateKeyPath": root.isTelnet ? "" : jumpKeyPathField.text,
            "forwards": root.isTelnet ? [] : _parseForwards(forwardsField.text)
        }
        if (appController.saveConnectionProfile(payload)) {
            root.saved()
            root.close()
        } else {
            validationMessage = appController.lastError
        }
    }

    background: Rectangle {
        color: theme.panel
        border.color: theme.borderMuted
        radius: 6
    }

    header: Rectangle {
        color: theme.surface
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
            color: theme.border
        }

        Label {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 16
            text: root.title
            color: theme.textPrimary
            font.pixelSize: 14
            font.bold: true
        }
    }

    footer: Rectangle {
        height: 56
        color: theme.surface
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
            color: theme.border
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
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            clip: true
            contentWidth: availableWidth
            ScrollBar.vertical.policy: ScrollBar.AsNeeded

        GridLayout {
            width: parent.width
            columns: 2
            columnSpacing: 12
            rowSpacing: 8

            Label { text: qsTr("Name"); color: theme.textMuted; font.pixelSize: 12 }
            ThemedTextField {
                id: nameField
                classic: root.classic
                error: root.validationShown && text.trim().length === 0
                Layout.fillWidth: true
                placeholderText: error ? qsTr("Connection name is required") : ""
                onTextChanged: if (nameField.text.trim().length > 0 && hostField.text.trim().length > 0) root.validationShown = false
            }

            Label { text: qsTr("Protocol"); color: theme.textMuted; font.pixelSize: 12 }
            ThemedComboBox {
                id: protocolBox
                classic: root.classic
                menuTheme: theme
                Layout.fillWidth: true
                model: root.protocols
                onActivated: {
                    if (currentText === "telnet" && portField.text === "22") {
                        portField.text = "23"
                    } else if (currentText !== "telnet" && portField.text === "23") {
                        portField.text = "22"
                    }
                }
            }

            Label {
                Layout.columnSpan: 2
                Layout.fillWidth: true
                visible: root.isTelnet
                text: qsTr("Telnet sends data in plaintext. Use it only on trusted networks.")
                color: theme.warning
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }

            Label { text: qsTr("Host"); color: theme.textMuted; font.pixelSize: 12 }
            ThemedTextField {
                id: hostField
                classic: root.classic
                error: root.validationShown && text.trim().length === 0
                Layout.fillWidth: true
                placeholderText: error ? qsTr("Host is required") : qsTr("Hostname or IP")
                onTextChanged: if (nameField.text.trim().length > 0 && hostField.text.trim().length > 0) root.validationShown = false
            }

            Label { text: qsTr("Port"); color: theme.textMuted; font.pixelSize: 12 }
            ThemedTextField {
                id: portField
                classic: root.classic
                Layout.preferredWidth: 88
                horizontalAlignment: TextInput.AlignHCenter
                inputMethodHints: Qt.ImhDigitsOnly
                validator: IntValidator { bottom: 1; top: 65535 }
                onEditingFinished: {
                    const port = Math.max(1, Math.min(65535, Number(text) || 22))
                    text = String(port)
                }
            }

            Label { text: qsTr("Username"); color: theme.textMuted; font.pixelSize: 12 }
            ThemedTextField { id: userField; classic: root.classic; Layout.fillWidth: true }

            Label { text: qsTr("Auth Method"); color: theme.textMuted; font.pixelSize: 12 }
            ThemedComboBox {
                id: authBox
                classic: root.classic
                menuTheme: theme
                Layout.fillWidth: true
                model: ["password", "key", "agent"]
                enabled: !root.isTelnet
                opacity: enabled ? 1.0 : 0.4
            }

            Label { text: qsTr("Password"); color: theme.textMuted; font.pixelSize: 12 }
            ThemedTextField {
                id: passwordField
                classic: root.classic
                Layout.fillWidth: true
                echoMode: TextInput.Password
                enabled: root.isTelnet || authBox.currentText === "password"
                opacity: enabled ? 1.0 : 0.4
                placeholderText: root.isTelnet ? qsTr("Optional auto-login password") : ""
            }

            Label { text: qsTr("Private Key"); color: theme.textMuted; font.pixelSize: 12 }
            ThemedTextField {
                id: keyPathField
                classic: root.classic
                Layout.fillWidth: true
                placeholderText: qsTr("Absolute path to .pem / .ppk")
                enabled: !root.isTelnet && authBox.currentText === "key"
                opacity: enabled ? 1.0 : 0.4
            }

            Label { text: qsTr("Group"); color: theme.textMuted; font.pixelSize: 12 }
            ThemedTextField { id: groupField; classic: root.classic; Layout.fillWidth: true }

            Label {
                text: qsTr("Notes")
                color: theme.textMuted
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

            Label {
                text: qsTr("Auto Reconnect")
                color: theme.textMuted
                font.pixelSize: 12
            }
            CheckBox {
                id: autoReconnectCheck
                checked: true
                text: qsTr("Reconnect with exponential backoff on disconnect")
            }

            Label {
                Layout.columnSpan: 2
                visible: root.isTelnet
                text: qsTr("Telnet Options")
                color: theme.textMuted
                font.pixelSize: 12
                Layout.topMargin: 6
            }

            Label {
                visible: root.isTelnet
                text: qsTr("Auto Login")
                color: theme.textMuted
                font.pixelSize: 12
            }
            CheckBox {
                id: telnetAutoLoginCheck
                visible: root.isTelnet
                checked: true
                text: qsTr("Send username/password when login prompts are detected")
            }

            Label {
                visible: root.isTelnet
                text: qsTr("Terminal Type")
                color: theme.textMuted
                font.pixelSize: 12
            }
            ThemedTextField {
                id: telnetTerminalTypeField
                visible: root.isTelnet
                classic: root.classic
                Layout.fillWidth: true
                text: "xterm-256color"
            }

            Label {
                Layout.columnSpan: 2
                text: qsTr("Jump Host (ProxyJump) — leave blank for direct connect")
                color: theme.textMuted
                font.pixelSize: 12
                Layout.topMargin: 6
            }

            Label { text: qsTr("Jump Host"); color: theme.textMuted; font.pixelSize: 12 }
            ThemedTextField {
                id: jumpHostField
                classic: root.classic
                Layout.fillWidth: true
                placeholderText: qsTr("Hostname or IP of bastion")
                enabled: !root.isTelnet
                opacity: enabled ? 1.0 : 0.4
            }

            Label { text: qsTr("Jump Port"); color: theme.textMuted; font.pixelSize: 12 }
            ThemedTextField {
                id: jumpPortField
                classic: root.classic
                Layout.preferredWidth: 88
                horizontalAlignment: TextInput.AlignHCenter
                inputMethodHints: Qt.ImhDigitsOnly
                validator: IntValidator { bottom: 1; top: 65535 }
                text: "22"
                enabled: !root.isTelnet
                opacity: enabled ? 1.0 : 0.4
            }

            Label { text: qsTr("Jump Username"); color: theme.textMuted; font.pixelSize: 12 }
            ThemedTextField {
                id: jumpUserField
                classic: root.classic
                Layout.fillWidth: true
                enabled: !root.isTelnet
                opacity: enabled ? 1.0 : 0.4
            }

            Label { text: qsTr("Jump Auth"); color: theme.textMuted; font.pixelSize: 12 }
            ThemedComboBox {
                id: jumpAuthBox
                classic: root.classic
                menuTheme: theme
                Layout.fillWidth: true
                model: ["password", "key", "agent"]
                enabled: !root.isTelnet
                opacity: enabled ? 1.0 : 0.4
            }

            Label { text: qsTr("Jump Password"); color: theme.textMuted; font.pixelSize: 12 }
            ThemedTextField {
                id: jumpPasswordField
                classic: root.classic
                Layout.fillWidth: true
                echoMode: TextInput.Password
                enabled: !root.isTelnet && jumpAuthBox.currentText === "password"
                opacity: enabled ? 1.0 : 0.4
            }

            Label { text: qsTr("Jump Private Key"); color: theme.textMuted; font.pixelSize: 12 }
            ThemedTextField {
                id: jumpKeyPathField
                classic: root.classic
                Layout.fillWidth: true
                placeholderText: qsTr("Absolute path")
                enabled: !root.isTelnet && jumpAuthBox.currentText === "key"
                opacity: enabled ? 1.0 : 0.4
            }

            Label {
                Layout.columnSpan: 2
                text: qsTr("Local Port Forwards — one per line: 127.0.0.1:8080 -> example.com:80")
                color: theme.textMuted
                font.pixelSize: 12
                Layout.topMargin: 6
            }
            Label {
                text: qsTr("Forwards")
                color: theme.textMuted
                font.pixelSize: 12
                Layout.alignment: Qt.AlignTop
            }
            ScrollView {
                id: forwardsScroll
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                clip: true
                contentWidth: availableWidth
                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                ThemedTextArea {
                    id: forwardsField
                    classic: root.classic
                    width: forwardsScroll.availableWidth
                    height: Math.max(forwardsScroll.availableHeight, implicitHeight)
                    enabled: !root.isTelnet
                    opacity: enabled ? 1.0 : 0.4
                }
            }
        }
        } // ScrollView

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 20
            radius: 3
            color: root.validationMessage.length > 0
                   ? theme.dangerSoft
                   : "transparent"

            Label {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                text: root.validationMessage
                color: theme.danger
                font.pixelSize: 12
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }
    }
}
