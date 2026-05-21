import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    required property var fileBrowser

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
                    color: "#020617"
                    font.bold: true
                    font.pixelSize: 16
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }

                Label {
                    id: symbolicLabel
                    text: "----------"
                    color: "#64748b"
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
                Label { text: qsTr("Read"); font.pixelSize: 12 }
                Label { text: qsTr("Write"); font.pixelSize: 12 }
                Label { text: qsTr("Exec"); font.pixelSize: 12 }

                Label { text: qsTr("Owner"); font.pixelSize: 12 }
                CheckBox { id: ownerRead; onCheckedChanged: chmodDialog.updateSymbolic() }
                CheckBox { id: ownerWrite; onCheckedChanged: chmodDialog.updateSymbolic() }
                CheckBox { id: ownerExec; onCheckedChanged: chmodDialog.updateSymbolic() }

                Label { text: qsTr("Group"); font.pixelSize: 12 }
                CheckBox { id: groupRead; onCheckedChanged: chmodDialog.updateSymbolic() }
                CheckBox { id: groupWrite; onCheckedChanged: chmodDialog.updateSymbolic() }
                CheckBox { id: groupExec; onCheckedChanged: chmodDialog.updateSymbolic() }

                Label { text: qsTr("Other"); font.pixelSize: 12 }
                CheckBox { id: otherRead; onCheckedChanged: chmodDialog.updateSymbolic() }
                CheckBox { id: otherWrite; onCheckedChanged: chmodDialog.updateSymbolic() }
                CheckBox { id: otherExec; onCheckedChanged: chmodDialog.updateSymbolic() }
            }

            GroupBox {
                Layout.fillWidth: true
                visible: root.fileBrowser.pendingChmodIsDir

                ColumnLayout {
                    anchors.fill: parent

                    CheckBox {
                        id: recursiveCheck
                        text: qsTr("Apply recursively")
                    }

                    RadioButton {
                        text: qsTr("Apply to files and folders")
                        checked: true
                    }

                    RadioButton {
                        text: qsTr("Apply to files only")
                    }

                    RadioButton {
                        text: qsTr("Apply to folders only")
                    }
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight

                Button {
                    text: qsTr("OK")
                    onClicked: chmodDialog.accept()
                }

                Button {
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
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: parent
        width: 300

        ColumnLayout {
            anchors.fill: parent
            spacing: 8

            Label {
                text: root.fileBrowser.nameDialogMode === "newDir" ? qsTr("Folder name")
                    : root.fileBrowser.nameDialogMode === "newFile" ? qsTr("File name")
                    : qsTr("New name")
                color: "#334155"
            }

            TextField {
                id: nameField
                Layout.fillWidth: true
                selectByMouse: true
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
        standardButtons: Dialog.Ok | Dialog.Cancel

        ColumnLayout {
            anchors.fill: parent
            spacing: 8

            Label {
                text: qsTr("Open remote files with")
                color: "#020617"
                font.pixelSize: 12
            }

            ComboBox {
                id: openModeBox
                Layout.fillWidth: true
                textRole: "label"
                valueRole: "value"
                model: [
                    { label: qsTr("System default app"), value: "system" },
                    { label: qsTr("Specified text editor"), value: "custom" },
                    { label: qsTr("Built-in editor"), value: "internal" }
                ]
            }

            RowLayout {
                Layout.fillWidth: true
                enabled: openModeBox.currentValue === "custom"

                TextField {
                    id: editorPathField
                    Layout.fillWidth: true
                    selectByMouse: true
                    placeholderText: qsTr("Text editor path")
                }

                Button {
                    text: qsTr("Browse")
                    onClicked: {
                        const path = appController.chooseExternalTextEditor()
                        if (path && path.length > 0) {
                            editorPathField.text = path
                        }
                    }
                }
            }

            CheckBox {
                id: autoUploadCheck
                text: qsTr("Auto upload after external edits")
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
            font.family: "Consolas"
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
