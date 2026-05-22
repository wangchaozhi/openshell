import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    required property var fileBrowser

    component DialogButton: Button {
        id: dialogButton

        property bool primary: false

        implicitWidth: 96
        implicitHeight: 34
        hoverEnabled: true

        contentItem: Label {
            text: dialogButton.text
            color: dialogButton.primary ? root.fileBrowser.theme.textOnAccent : root.fileBrowser.theme.textPrimary
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 13
        }

        background: Rectangle {
            radius: 4
            color: dialogButton.primary
                   ? (dialogButton.hovered ? root.fileBrowser.theme.focus : root.fileBrowser.theme.textActive)
                   : (dialogButton.hovered ? root.fileBrowser.theme.hover : root.fileBrowser.theme.surfaceRaised)
            border.width: dialogButton.primary ? 0 : 1
            border.color: root.fileBrowser.theme.borderMuted
        }
    }

    component DialogCheckBox: CheckBox {
        id: dialogCheck

        spacing: 8

        indicator: Rectangle {
            implicitWidth: 20
            implicitHeight: 20
            x: dialogCheck.leftPadding
            y: parent.height / 2 - height / 2
            radius: 3
            color: dialogCheck.checked ? root.fileBrowser.theme.textActive : root.fileBrowser.theme.surface
            border.width: 1
            border.color: dialogCheck.activeFocus ? root.fileBrowser.theme.focus : root.fileBrowser.theme.borderMuted

            Label {
                anchors.centerIn: parent
                text: dialogCheck.checked ? "\u2713" : ""
                color: root.fileBrowser.theme.textOnAccent
                font.pixelSize: 16
                font.bold: true
            }
        }

        contentItem: Label {
            text: dialogCheck.text
            color: dialogCheck.enabled ? root.fileBrowser.theme.textPrimary : root.fileBrowser.theme.textMuted
            verticalAlignment: Text.AlignVCenter
            leftPadding: dialogCheck.indicator.width + dialogCheck.spacing
            font.pixelSize: 13
        }
    }

    component DialogRadioButton: RadioButton {
        id: dialogRadio

        spacing: 8

        indicator: Rectangle {
            implicitWidth: 18
            implicitHeight: 18
            x: dialogRadio.leftPadding
            y: parent.height / 2 - height / 2
            radius: width / 2
            color: root.fileBrowser.theme.surface
            border.width: 1
            border.color: dialogRadio.activeFocus ? root.fileBrowser.theme.focus : root.fileBrowser.theme.borderMuted

            Rectangle {
                anchors.centerIn: parent
                width: 8
                height: 8
                radius: 4
                visible: dialogRadio.checked
                color: root.fileBrowser.theme.textActive
            }
        }

        contentItem: Label {
            text: dialogRadio.text
            color: dialogRadio.enabled ? root.fileBrowser.theme.textPrimary : root.fileBrowser.theme.textMuted
            verticalAlignment: Text.AlignVCenter
            leftPadding: dialogRadio.indicator.width + dialogRadio.spacing
            font.pixelSize: 13
        }
    }

    function openChmod(currentPermissions) {
        chmodDialog.applyOctal(currentPermissions && currentPermissions.length > 0
                               ? currentPermissions
                               : (fileBrowser.pendingChmodIsDir ? "755" : "644"))
        chmodDialog.open()
    }

    function openName(initialName) {
        nameField.text = initialName || ""
        nameField.selectAll()
        nameDialog.open()
    }

    function openRemoteOpenSettings() {
        openModeBox.currentIndex = Math.max(0, openModeBox.indexOfValue(appController.remoteFileOpenMode))
        editorPathField.text = appController.externalTextEditorPath
        autoUploadCheck.checked = appController.autoUploadRemoteEdits
        remoteOpenSettingsDialog.open()
    }

    function openInternalEditor(text) {
        internalEditorText.text = text
        internalEditorDialog.open()
    }

    Dialog {
        id: chmodDialog
        title: qsTr("Change File Permissions")
        modal: true
        anchors.centerIn: parent
        width: 280
        padding: 12
        margins: 12

        background: Rectangle {
            color: root.fileBrowser.theme.panel
            border.width: 1
            border.color: root.fileBrowser.theme.borderMuted
            radius: 4
        }

        header: Rectangle {
            implicitHeight: 46
            color: root.fileBrowser.theme.surface

            Label {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                text: chmodDialog.title
                color: root.fileBrowser.theme.textPrimary
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 14
                font.bold: true
            }
        }

        function applyOctal(value) {
            const normalized = (value && value.length > 0 ? value : "755").slice(-3)
            const owner = parseInt(normalized.charAt(0))
            const group = parseInt(normalized.charAt(1))
            const other = parseInt(normalized.charAt(2))
            ownerRead.checked = (owner & 4) !== 0
            ownerWrite.checked = (owner & 2) !== 0
            ownerExec.checked = (owner & 1) !== 0
            groupRead.checked = (group & 4) !== 0
            groupWrite.checked = (group & 2) !== 0
            groupExec.checked = (group & 1) !== 0
            otherRead.checked = (other & 4) !== 0
            otherWrite.checked = (other & 2) !== 0
            otherExec.checked = (other & 1) !== 0
            updateSymbolic()
        }

        function octalText() {
            const owner = (ownerRead.checked ? 4 : 0)
                        + (ownerWrite.checked ? 2 : 0)
                        + (ownerExec.checked ? 1 : 0)
            const group = (groupRead.checked ? 4 : 0)
                        + (groupWrite.checked ? 2 : 0)
                        + (groupExec.checked ? 1 : 0)
            const other = (otherRead.checked ? 4 : 0)
                        + (otherWrite.checked ? 2 : 0)
                        + (otherExec.checked ? 1 : 0)
            return "" + owner + group + other
        }

        function updateSymbolic() {
            const prefix = root.fileBrowser.pendingChmodIsDir ? "d" : "-"
            const owner = (ownerRead.checked ? "r" : "-")
                        + (ownerWrite.checked ? "w" : "-")
                        + (ownerExec.checked ? "x" : "-")
            const group = (groupRead.checked ? "r" : "-")
                        + (groupWrite.checked ? "w" : "-")
                        + (groupExec.checked ? "x" : "-")
            const other = (otherRead.checked ? "r" : "-")
                        + (otherWrite.checked ? "w" : "-")
                        + (otherExec.checked ? "x" : "-")
            symbolicLabel.text = prefix + owner + group + other
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 6

            RowLayout {
                Layout.fillWidth: true

                Label {
                    text: root.fileBrowser.pendingChmodName
                    color: root.fileBrowser.theme.textPrimary
                    font.bold: true
                    font.pixelSize: 16
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }

                Label {
                    id: symbolicLabel
                    text: "----------"
                    color: root.fileBrowser.theme.textMuted
                    font.family: "Courier New"
                    font.pixelSize: 12
                    font.bold: true
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 4
                columnSpacing: 4
                rowSpacing: 2

                Item { Layout.preferredWidth: 44 }
                Label { text: qsTr("Read"); color: root.fileBrowser.theme.textSecondary; font.pixelSize: 12 }
                Label { text: qsTr("Write"); color: root.fileBrowser.theme.textSecondary; font.pixelSize: 12 }
                Label { text: qsTr("Exec"); color: root.fileBrowser.theme.textSecondary; font.pixelSize: 12 }

                Label { text: qsTr("Owner"); color: root.fileBrowser.theme.textSecondary; font.pixelSize: 12 }
                DialogCheckBox { id: ownerRead; onCheckedChanged: chmodDialog.updateSymbolic() }
                DialogCheckBox { id: ownerWrite; onCheckedChanged: chmodDialog.updateSymbolic() }
                DialogCheckBox { id: ownerExec; onCheckedChanged: chmodDialog.updateSymbolic() }

                Label { text: qsTr("Group"); color: root.fileBrowser.theme.textSecondary; font.pixelSize: 12 }
                DialogCheckBox { id: groupRead; onCheckedChanged: chmodDialog.updateSymbolic() }
                DialogCheckBox { id: groupWrite; onCheckedChanged: chmodDialog.updateSymbolic() }
                DialogCheckBox { id: groupExec; onCheckedChanged: chmodDialog.updateSymbolic() }

                Label { text: qsTr("Other"); color: root.fileBrowser.theme.textSecondary; font.pixelSize: 12 }
                DialogCheckBox { id: otherRead; onCheckedChanged: chmodDialog.updateSymbolic() }
                DialogCheckBox { id: otherWrite; onCheckedChanged: chmodDialog.updateSymbolic() }
                DialogCheckBox { id: otherExec; onCheckedChanged: chmodDialog.updateSymbolic() }
            }

            GroupBox {
                Layout.fillWidth: true
                visible: root.fileBrowser.pendingChmodIsDir
                label: Label {
                    text: qsTr("Scope")
                    color: root.fileBrowser.theme.textSecondary
                    font.pixelSize: 12
                }
                background: Rectangle {
                    color: root.fileBrowser.theme.surface
                    border.color: root.fileBrowser.theme.borderMuted
                    radius: 4
                }

                ColumnLayout {
                    anchors.fill: parent

                    DialogCheckBox {
                        id: recursiveCheck
                        text: qsTr("Apply recursively")
                    }

                    DialogRadioButton {
                        text: qsTr("Apply to files and folders")
                        checked: true
                    }

                    DialogRadioButton {
                        text: qsTr("Apply to files only")
                    }

                    DialogRadioButton {
                        text: qsTr("Apply to folders only")
                    }
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight

                DialogButton {
                    text: qsTr("OK")
                    primary: true
                    onClicked: chmodDialog.accept()
                }

                DialogButton {
                    text: qsTr("Cancel")
                    onClicked: chmodDialog.reject()
                }
            }
        }

        onAccepted: {
            root.fileBrowser.startRemoteOperation(appController.requestRemoteChmod(root.fileBrowser.connectionId,
                                                                                   root.fileBrowser.pendingChmodPath,
                                                                                   octalText()))
        }
    }

    Dialog {
        id: nameDialog
        title: root.fileBrowser.nameDialogMode === "rename" ? qsTr("Rename") : qsTr("New")
        modal: true
        anchors.centerIn: parent
        width: 300
        padding: 12
        margins: 12

        background: Rectangle {
            color: root.fileBrowser.theme.panel
            border.width: 1
            border.color: root.fileBrowser.theme.borderMuted
            radius: 4
        }

        header: Rectangle {
            implicitHeight: 46
            color: root.fileBrowser.theme.surface

            Label {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                text: nameDialog.title
                color: root.fileBrowser.theme.textPrimary
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 14
                font.bold: true
            }
        }

        footer: Rectangle {
            implicitHeight: 58
            color: root.fileBrowser.theme.panel

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Item { Layout.fillWidth: true }

                DialogButton {
                    text: qsTr("OK")
                    primary: true
                    onClicked: nameDialog.accept()
                }

                DialogButton {
                    text: qsTr("Cancel")
                    onClicked: nameDialog.reject()
                }
            }
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 8

            Label {
                text: root.fileBrowser.nameDialogMode === "newDir" ? qsTr("Folder name")
                    : root.fileBrowser.nameDialogMode === "newFile" ? qsTr("File name")
                    : qsTr("New name")
                color: root.fileBrowser.theme.textSecondary
            }

            TextField {
                id: nameField
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                selectByMouse: true
                color: root.fileBrowser.theme.textPrimary
                placeholderTextColor: root.fileBrowser.theme.textMuted
                selectedTextColor: root.fileBrowser.theme.textOnAccent
                selectionColor: root.fileBrowser.theme.textActive
                background: Rectangle {
                    color: root.fileBrowser.theme.surfaceRaised
                    border.width: 1
                    border.color: nameField.activeFocus
                                  ? root.fileBrowser.theme.focus
                                  : root.fileBrowser.theme.borderMuted
                    radius: 4
                }
            }
        }

        onAccepted: {
            if (nameField.text.length === 0) {
                return
            }
            if (root.fileBrowser.nameDialogMode === "rename") {
                root.fileBrowser.startRemoteOperation(appController.requestRenameRemotePath(root.fileBrowser.connectionId,
                                                                                           root.fileBrowser.pendingRemotePath,
                                                                                           nameField.text))
            } else {
                root.fileBrowser.startRemoteOperation(appController.requestCreateRemotePath(root.fileBrowser.connectionId,
                                                                                           root.fileBrowser.remotePath,
                                                                                           nameField.text,
                                                                                           root.fileBrowser.nameDialogMode === "newDir"))
            }
        }
    }

    Dialog {
        id: remoteOpenSettingsDialog
        title: qsTr("Remote File Open")
        modal: true
        anchors.centerIn: parent
        width: 420
        padding: 0
        margins: 12

        background: Rectangle {
            color: root.fileBrowser.theme.panel
            border.width: 1
            border.color: root.fileBrowser.theme.borderMuted
            radius: 4
        }

        header: Rectangle {
            implicitHeight: 46
            color: root.fileBrowser.theme.surface

            Label {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                text: remoteOpenSettingsDialog.title
                color: root.fileBrowser.theme.textPrimary
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 14
                font.bold: true
            }
        }

        contentItem: ColumnLayout {
            implicitWidth: remoteOpenSettingsDialog.width
            implicitHeight: 142
            spacing: 10

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 12
            }

            Label {
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Layout.fillWidth: true
                text: qsTr("Open remote files with")
                color: root.fileBrowser.theme.textSecondary
                font.pixelSize: 12
            }

            ComboBox {
                id: openModeBox
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                textRole: "label"
                valueRole: "value"
                model: [
                    { label: qsTr("System default app"), value: "system" },
                    { label: qsTr("Specified text editor"), value: "custom" },
                    { label: qsTr("Built-in editor"), value: "internal" }
                ]

                contentItem: Label {
                    leftPadding: 10
                    rightPadding: 28
                    text: openModeBox.displayText
                    color: root.fileBrowser.theme.textPrimary
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }

                indicator: Label {
                    x: openModeBox.width - width - 10
                    y: (openModeBox.height - height) / 2
                    text: "\u2039\n\u203a"
                    color: root.fileBrowser.theme.icon
                    rotation: 90
                    font.pixelSize: 17
                    lineHeight: 0.55
                }

                background: Rectangle {
                    color: root.fileBrowser.theme.surfaceRaised
                    border.width: 1
                    border.color: openModeBox.activeFocus || openModeBox.down
                                  ? root.fileBrowser.theme.focus
                                  : root.fileBrowser.theme.borderMuted
                    radius: 4
                }

                delegate: ItemDelegate {
                    width: openModeBox.width
                    height: 30
                    highlighted: openModeBox.highlightedIndex === index

                    contentItem: Label {
                        text: modelData.label
                        color: highlighted ? root.fileBrowser.theme.textOnAccent : root.fileBrowser.theme.textPrimary
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 13
                    }

                    background: Rectangle {
                        color: highlighted ? root.fileBrowser.theme.textActive : root.fileBrowser.theme.surfaceRaised
                    }
                }

                popup: Popup {
                    y: openModeBox.height + 2
                    width: openModeBox.width
                    implicitHeight: Math.min(contentItem.implicitHeight + 2, 120)
                    padding: 1

                    contentItem: ListView {
                        clip: true
                        implicitHeight: contentHeight
                        model: openModeBox.popup.visible ? openModeBox.delegateModel : null
                        currentIndex: openModeBox.highlightedIndex
                    }

                    background: Rectangle {
                        color: root.fileBrowser.theme.surfaceRaised
                        border.color: root.fileBrowser.theme.borderMuted
                        radius: 4
                    }
                }
            }

            RowLayout {
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Layout.fillWidth: true
                enabled: openModeBox.currentValue === "custom"
                opacity: enabled ? 1 : 0.62

                TextField {
                    id: editorPathField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    selectByMouse: true
                    placeholderText: qsTr("Text editor path")
                    color: root.fileBrowser.theme.textPrimary
                    placeholderTextColor: root.fileBrowser.theme.textMuted
                    selectedTextColor: root.fileBrowser.theme.textOnAccent
                    selectionColor: root.fileBrowser.theme.textActive

                    background: Rectangle {
                        color: root.fileBrowser.theme.surfaceRaised
                        border.width: 1
                        border.color: editorPathField.activeFocus
                                      ? root.fileBrowser.theme.focus
                                      : root.fileBrowser.theme.borderMuted
                        radius: 4
                    }
                }

                DialogButton {
                    text: qsTr("Browse")
                    onClicked: {
                        const path = appController.chooseExternalTextEditor()
                        if (path && path.length > 0) {
                            editorPathField.text = path
                        }
                    }
                }
            }

            DialogCheckBox {
                id: autoUploadCheck
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Layout.fillWidth: true
                text: qsTr("Auto upload after external edits")
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 2
            }
        }

        footer: Rectangle {
            implicitHeight: 58
            color: root.fileBrowser.theme.panel

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                anchors.topMargin: 10
                anchors.bottomMargin: 12
                spacing: 1

                Item { Layout.fillWidth: true }

                DialogButton {
                    primary: true
                    text: qsTr("OK")
                    onClicked: remoteOpenSettingsDialog.accept()
                }

                DialogButton {
                    text: qsTr("Cancel")
                    onClicked: remoteOpenSettingsDialog.reject()
                }
            }
        }

        onAccepted: {
            appController.remoteFileOpenMode = openModeBox.currentValue || "system"
            appController.externalTextEditorPath = editorPathField.text
            appController.autoUploadRemoteEdits = autoUploadCheck.checked
        }
    }

    Dialog {
        id: internalEditorDialog
        title: root.fileBrowser.editingRemotePath.length > 0
               ? root.fileBrowser.editingRemotePath
               : qsTr("Built-in editor")
        modal: false
        anchors.centerIn: parent
        width: Math.min(root.fileBrowser.width - 48, 820)
        height: Math.min(root.fileBrowser.height - 36, 520)
        standardButtons: Dialog.Save | Dialog.Close

        TextArea {
            id: internalEditorText
            anchors.fill: parent
            selectByMouse: true
            wrapMode: TextEdit.NoWrap
            font.family: Qt.platform.os === "osx" ? "Menlo" : "Consolas"
            font.pixelSize: 13
        }

        onAccepted: {
            if (appController.saveTextFile(root.fileBrowser.editingLocalPath, internalEditorText.text)) {
                root.fileBrowser.startRemoteOperation(appController.requestUploadEditedRemoteFile(root.fileBrowser.editingConnectionId,
                                                                                                 root.fileBrowser.editingLocalPath,
                                                                                                 root.fileBrowser.editingRemotePath))
            }
        }
    }
}
