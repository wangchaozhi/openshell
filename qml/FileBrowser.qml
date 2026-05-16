import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var session: ({})
    property string localPath: appController.localHomePath()
    property var localEntries: []
    property string remotePath: ""
    property var remoteEntries: []
    property string remoteError: ""
    property bool remoteLoading: false
    property string remoteRequestId: ""
    property string remoteOperationRequestId: ""
    property string pendingChmodPath: ""
    property string pendingChmodName: ""
    property string pendingUploadPath: ""
    property string pendingRemotePath: ""
    property string nameDialogMode: ""
    readonly property string connectionId: session && session.connectionId ? session.connectionId : ""

    color: "#0f172a"
    border.color: "#1e293b"

    component ParentIcon: Item {
        implicitWidth: 15
        implicitHeight: 15
        Rectangle {
            x: 2
            y: 3
            width: 11
            height: 9
            radius: 2
            color: "#93c5fd"
        }
        Rectangle {
            x: 2
            y: 1
            width: 6
            height: 4
            radius: 1
            color: "#60a5fa"
        }
        Rectangle {
            x: 4
            y: 7
            width: 7
            height: 2
            radius: 1
            color: "#0f172a"
        }
        Rectangle {
            x: 4
            y: 7
            width: 2
            height: 5
            radius: 1
            color: "#0f172a"
        }
    }

    component RefreshIcon: Item {
        implicitWidth: 15
        implicitHeight: 15
        Rectangle {
            x: 2
            y: 2
            width: 11
            height: 11
            radius: 6
            color: "transparent"
            border.color: "#93c5fd"
            border.width: 2
        }
        Rectangle {
            x: 9
            y: 1
            width: 5
            height: 5
            rotation: 45
            color: "#93c5fd"
        }
        Rectangle {
            x: 0
            y: 9
            width: 5
            height: 5
            rotation: 45
            color: "#93c5fd"
        }
        Rectangle {
            x: 1
            y: 6
            width: 5
            height: 4
            color: "#020617"
        }
        Rectangle {
            x: 9
            y: 5
            width: 5
            height: 4
            color: "#020617"
        }
    }

    component FileTypeIcon: Item {
        property bool isDir: false

        implicitWidth: 16
        implicitHeight: 14

        Rectangle {
            visible: isDir
            x: 1
            y: 2
            width: 7
            height: 3
            radius: 1
            color: "#60a5fa"
        }
        Rectangle {
            visible: isDir
            x: 1
            y: 5
            width: 14
            height: 9
            radius: 2
            color: "#93c5fd"
        }
        Rectangle {
            visible: !isDir
            x: 3
            y: 1
            width: 10
            height: 13
            radius: 1
            color: "#94a3b8"
        }
        Rectangle {
            visible: !isDir
            x: 10
            y: 1
            width: 3
            height: 3
            color: "#cbd5e1"
        }
    }

    Component.onCompleted: refreshLocal()

    Dialog {
        id: chmodDialog
        title: qsTr("Change File Permissions")
        modal: true
        anchors.centerIn: parent
        width: 260

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

        ColumnLayout {
            anchors.fill: parent
            spacing: 6

            Label {
                Layout.fillWidth: true
                text: root.pendingChmodName
                color: "#020617"
                font.bold: true
                font.pixelSize: 16
                elide: Text.ElideMiddle
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
                CheckBox { id: ownerRead }
                CheckBox { id: ownerWrite }
                CheckBox { id: ownerExec }

                Label { text: qsTr("Group"); font.pixelSize: 12 }
                CheckBox { id: groupRead }
                CheckBox { id: groupWrite }
                CheckBox { id: groupExec }

                Label { text: qsTr("Other"); font.pixelSize: 12 }
                CheckBox { id: otherRead }
                CheckBox { id: otherWrite }
                CheckBox { id: otherExec }
            }

            GroupBox {
                Layout.fillWidth: true
                ColumnLayout {
                    anchors.fill: parent
                    CheckBox {
                        id: recursiveCheck
                        text: qsTr("Apply recursively")
                        enabled: false
                    }
                    RadioButton {
                        text: qsTr("Apply to files and folders")
                        enabled: false
                        checked: true
                    }
                    RadioButton {
                        text: qsTr("Apply to files only")
                        enabled: false
                    }
                    RadioButton {
                        text: qsTr("Apply to folders only")
                        enabled: false
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
            root.remoteOperationRequestId = appController.requestRemoteChmod(root.connectionId,
                                                                             root.pendingChmodPath,
                                                                             octalText())
            root.remoteLoading = true
            remoteRequestTimeout.restart()
        }
    }

    Dialog {
        id: nameDialog
        title: root.nameDialogMode === "rename" ? qsTr("Rename") : qsTr("New")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: parent
        width: 300

        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            Label {
                text: root.nameDialogMode === "newDir" ? qsTr("Folder name")
                    : root.nameDialogMode === "newFile" ? qsTr("File name")
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
            if (root.nameDialogMode === "rename") {
                root.remoteOperationRequestId = appController.requestRenameRemotePath(root.connectionId,
                                                                                     root.pendingRemotePath,
                                                                                     nameField.text)
            } else {
                root.remoteOperationRequestId = appController.requestCreateRemotePath(root.connectionId,
                                                                                     root.remotePath,
                                                                                     nameField.text,
                                                                                     root.nameDialogMode === "newDir")
            }
            root.remoteLoading = true
            remoteRequestTimeout.restart()
        }
    }

    Timer {
        id: remoteRequestTimeout
        interval: 12000
        repeat: false
        onTriggered: {
            if (!root.remoteLoading) {
                return
            }
            root.remoteLoading = false
            root.remoteRequestId = ""
            root.remoteOperationRequestId = ""
            root.remoteError = qsTr("Remote listing timed out. Try refresh again.")
        }
    }

    onConnectionIdChanged: {
        if (connectionId === "") {
            remotePath = ""
            remoteEntries = []
            remoteLoading = false
            remoteRequestId = ""
            remoteRequestTimeout.stop()
            remoteError = qsTr("Open a session to enable SFTP browsing.")
            return
        }
        remotePath = appController.remoteHomePath(connectionId)
        refreshRemote()
    }

    function refreshLocal() {
        localEntries = appController.localDirectoryEntries(localPath)
    }

    function enterLocal(path) {
        localPath = path
        refreshLocal()
    }

    function refreshRemote() {
        if (connectionId === "") {
            remoteEntries = []
            remoteLoading = false
            remoteRequestId = ""
            remoteRequestTimeout.stop()
            remoteError = qsTr("Open a session to enable SFTP browsing.")
            return
        }
        remoteError = ""
        remoteLoading = true
        remoteRequestId = appController.requestRemoteDirectoryEntries(connectionId, remotePath)
        remoteRequestTimeout.restart()
    }

    function enterRemote(path) {
        remotePath = path
        refreshRemote()
    }

    function uploadLocalPath(path) {
        if (connectionId === "" || path.length === 0) {
            return
        }
        pendingUploadPath = path
        remoteError = ""
        remoteLoading = true
        remoteOperationRequestId = appController.requestUploadLocalPath(connectionId,
                                                                        path,
                                                                        remotePath)
        remoteRequestTimeout.restart()
    }

    function chooseAndUploadFile() {
        const path = appController.chooseLocalFile()
        if (path && path.length > 0) {
            uploadLocalPath(path)
        }
    }

    function chooseAndUploadFolder() {
        const path = appController.chooseLocalFolder()
        if (path && path.length > 0) {
            uploadLocalPath(path)
        }
    }

    function changeRemotePermissions(path, permissions) {
        if (connectionId === "" || path.length === 0) {
            return
        }
        root.remoteOperationRequestId = appController.requestRemoteChmod(root.connectionId,
                                                                         path,
                                                                         permissions)
        root.remoteLoading = true
        remoteRequestTimeout.restart()
    }

    function openChmodDialog(path, currentPermissions) {
        if (connectionId === "" || path.length === 0) {
            return
        }
        pendingChmodPath = path
        const parts = path.split("/")
        pendingChmodName = parts.length > 0 && parts[parts.length - 1].length > 0
                         ? parts[parts.length - 1]
                         : path
        chmodDialog.applyOctal(currentPermissions && currentPermissions.length > 0
                               ? currentPermissions
                               : "755")
        chmodDialog.open()
    }

    function downloadRemotePath(path) {
        const folder = appController.chooseDownloadFolder()
        if (!folder || folder.length === 0) {
            return
        }
        remoteOperationRequestId = appController.requestRemoteDownload(connectionId, path, folder)
        remoteLoading = true
        remoteRequestTimeout.restart()
    }

    function openRemotePath(path) {
        remoteOperationRequestId = appController.requestOpenRemotePath(connectionId, path)
        remoteLoading = true
        remoteRequestTimeout.restart()
    }

    function openNameDialog(mode, path, currentName) {
        nameDialogMode = mode
        pendingRemotePath = path || ""
        nameField.text = currentName || ""
        nameField.selectAll()
        nameDialog.open()
    }

    function deleteRemotePath(path, recursive) {
        remoteOperationRequestId = appController.requestDeleteRemotePath(connectionId, path, recursive)
        remoteLoading = true
        remoteRequestTimeout.restart()
    }

    Connections {
        target: appController
        function onRemoteDirectoryEntriesReady(requestId, connectionId, path, entries, error) {
            if (requestId !== root.remoteRequestId || connectionId !== root.connectionId) {
                return
            }
            root.remoteLoading = false
            remoteRequestTimeout.stop()
            root.remotePath = path
            root.remoteEntries = entries
            root.remoteError = error || ""
        }

        function onRemoteOperationFinished(requestId, connectionId, operation, path, ok, message) {
            if (requestId !== root.remoteOperationRequestId || connectionId !== root.connectionId) {
                return
            }
            root.remoteOperationRequestId = ""
            root.remoteLoading = false
            remoteRequestTimeout.stop()
            root.remoteError = ok ? "" : message
            if (ok) {
                root.refreshRemote()
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 4

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#020617"
            radius: 4

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        text: qsTr("Local")
                        color: "#cbd5f5"
                        font.bold: true
                        font.pixelSize: 12
                    }

                    ToolButton {
                        implicitWidth: 30
                        implicitHeight: 24
                        contentItem: ParentIcon {
                            anchors.centerIn: parent
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Parent folder")
                        onClicked: {
                            root.localPath = appController.localParentPath(root.localPath)
                            root.refreshLocal()
                        }
                    }

                    ToolButton {
                        implicitWidth: 30
                        implicitHeight: 24
                        contentItem: RefreshIcon {
                            anchors.centerIn: parent
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Refresh")
                        onClicked: root.refreshLocal()
                    }
                }

                TextField {
                    Layout.fillWidth: true
                    text: root.localPath
                    selectByMouse: true
                    color: "#dbeafe"
                    font.pixelSize: 11
                    background: Rectangle {
                        color: "#0f172a"
                        border.color: "#1e293b"
                        radius: 3
                    }
                    onAccepted: {
                        root.localPath = text
                        root.refreshLocal()
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 22
                    color: "#0f172a"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 8

                        Label {
                            text: qsTr("Name")
                            color: "#93c5fd"
                            font.pixelSize: 11
                            Layout.fillWidth: true
                        }
                        Label {
                            text: qsTr("Size")
                            color: "#93c5fd"
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignRight
                            Layout.preferredWidth: 78
                        }
                        Label {
                            text: qsTr("Perm")
                            color: "#93c5fd"
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignRight
                            Layout.preferredWidth: 52
                        }
                        Label {
                            text: qsTr("Modified")
                            color: "#93c5fd"
                            font.pixelSize: 11
                            Layout.preferredWidth: 118
                        }
                    }
                }

                ListView {
                    id: localList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: root.localEntries

                    delegate: Rectangle {
                        required property var modelData

                        width: localList.width
                        height: 26
                        color: mouseArea.containsMouse ? "#111827" : "#020617"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 8

                            FileTypeIcon {
                                isDir: modelData.isDir
                                Layout.alignment: Qt.AlignVCenter
                            }

                            Label {
                                text: modelData.name
                                color: modelData.isDir ? "#bfdbfe" : "#d1d5db"
                                font.pixelSize: 11
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Label {
                                text: modelData.size
                                color: "#94a3b8"
                                font.pixelSize: 11
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: 78
                            }
                            Label {
                                text: modelData.modified
                                color: "#94a3b8"
                                font.pixelSize: 11
                                Layout.preferredWidth: 118
                            }
                        }

                        MouseArea {
                            id: mouseArea
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            hoverEnabled: true
                            onClicked: function(mouse) {
                                if (mouse.button === Qt.RightButton) {
                                    localItemMenu.popup()
                                }
                            }
                            onDoubleClicked: {
                                if (modelData.isDir) {
                                    root.enterLocal(modelData.path)
                                }
                            }

                            Menu {
                                id: localItemMenu
                                MenuItem {
                                    text: qsTr("Upload to Remote")
                                    enabled: root.connectionId !== ""
                                    onTriggered: root.uploadLocalPath(modelData.path)
                                }
                            }
                        }
                    }

                    ScrollBar.vertical: ScrollBar {}
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#020617"
            radius: 4

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        text: qsTr("Remote")
                        color: "#cbd5f5"
                        font.bold: true
                        font.pixelSize: 12
                    }

                    ToolButton {
                        implicitWidth: 30
                        implicitHeight: 24
                        enabled: root.connectionId !== ""
                        contentItem: ParentIcon {
                            anchors.centerIn: parent
                            opacity: parent.enabled ? 1 : 0.35
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Parent folder")
                        onClicked: {
                            root.remotePath = appController.remoteParentPath(root.remotePath)
                            root.refreshRemote()
                        }
                    }

                    ToolButton {
                        implicitWidth: 30
                        implicitHeight: 24
                        enabled: root.connectionId !== ""
                        contentItem: RefreshIcon {
                            anchors.centerIn: parent
                            opacity: parent.enabled ? 1 : 0.35
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Refresh")
                        onClicked: root.refreshRemote()
                    }
                }

                TextField {
                    Layout.fillWidth: true
                    text: root.remotePath
                    enabled: root.connectionId !== ""
                    selectByMouse: true
                    color: "#dbeafe"
                    font.pixelSize: 11
                    background: Rectangle {
                        color: "#0f172a"
                        border.color: "#1e293b"
                        radius: 3
                    }
                    onAccepted: {
                        root.remotePath = text
                        root.refreshRemote()
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 22
                    color: "#0f172a"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 8

                        Label {
                            text: qsTr("Name")
                            color: "#93c5fd"
                            font.pixelSize: 11
                            Layout.fillWidth: true
                        }
                        Label {
                            text: qsTr("Size")
                            color: "#93c5fd"
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignRight
                            Layout.preferredWidth: 78
                        }
                        Label {
                            text: qsTr("Perm")
                            color: "#93c5fd"
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignRight
                            Layout.preferredWidth: 52
                        }
                        Label {
                            text: qsTr("Modified")
                            color: "#93c5fd"
                            font.pixelSize: 11
                            Layout.preferredWidth: 118
                        }
                    }
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: root.remoteLoading || root.remoteError.length > 0 ? 1 : 0

                    ListView {
                        id: remoteList
                        clip: true
                        model: root.remoteEntries

                        delegate: Rectangle {
                            required property var modelData

                            width: remoteList.width
                            height: 26
                            color: remoteMouseArea.containsMouse ? "#111827" : "#020617"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 8

                                FileTypeIcon {
                                    isDir: modelData.isDir
                                    Layout.alignment: Qt.AlignVCenter
                                }

                                Label {
                                    text: modelData.name
                                    color: modelData.isDir ? "#bfdbfe" : "#d1d5db"
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: modelData.size
                                    color: "#94a3b8"
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignRight
                                    Layout.preferredWidth: 78
                                }
                                Label {
                                    text: modelData.permissions || ""
                                    color: "#94a3b8"
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignRight
                                    Layout.preferredWidth: 52
                                }
                                Label {
                                    text: modelData.modified
                                    color: "#94a3b8"
                                    font.pixelSize: 11
                                    Layout.preferredWidth: 118
                                }
                            }

                            MouseArea {
                                id: remoteMouseArea
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                hoverEnabled: true
                                onClicked: function(mouse) {
                                    if (mouse.button === Qt.RightButton) {
                                        remoteItemMenu.popup()
                                    }
                                }
                                onDoubleClicked: {
                                    if (modelData.isDir) {
                                        root.enterRemote(modelData.path)
                                    }
                                }

                                Menu {
                                    id: remoteItemMenu
                                    MenuItem {
                                        text: qsTr("Refresh")
                                        onTriggered: root.refreshRemote()
                                    }
                                    MenuSeparator {}
                                    MenuItem {
                                        text: qsTr("Open")
                                        enabled: !modelData.isDir
                                        onTriggered: root.openRemotePath(modelData.path)
                                    }
                                    Menu {
                                        title: qsTr("Open With")
                                        enabled: false
                                        MenuItem { text: qsTr("Not configured") }
                                    }
                                    Menu {
                                        title: qsTr("Select Text Editor")
                                        enabled: false
                                        MenuItem { text: qsTr("Coming soon") }
                                    }
                                    MenuSeparator {}
                                    MenuItem {
                                        text: qsTr("Copy Path")
                                        onTriggered: appController.copyTextToClipboard(modelData.path)
                                    }
                                    MenuSeparator {}
                                    MenuItem {
                                        text: qsTr("Download")
                                        onTriggered: root.downloadRemotePath(modelData.path)
                                    }
                                    MenuItem {
                                        text: qsTr("Upload...")
                                        enabled: modelData.isDir
                                        onTriggered: {
                                            root.remotePath = modelData.path
                                            root.chooseAndUploadFile()
                                        }
                                    }
                                    Menu {
                                        title: qsTr("Transfer Package")
                                        enabled: false
                                        MenuItem { text: qsTr("Coming soon") }
                                    }
                                    MenuSeparator {}
                                    Menu {
                                        title: qsTr("New")
                                        MenuItem {
                                            text: qsTr("File")
                                            onTriggered: root.openNameDialog("newFile", "", "new-file")
                                        }
                                        MenuItem {
                                            text: qsTr("Folder")
                                            onTriggered: root.openNameDialog("newDir", "", "new-folder")
                                        }
                                    }
                                    MenuSeparator {}
                                    MenuItem {
                                        text: qsTr("Rename")
                                        onTriggered: root.openNameDialog("rename", modelData.path, modelData.name)
                                    }
                                    MenuItem {
                                        text: qsTr("Delete")
                                        onTriggered: root.deleteRemotePath(modelData.path, false)
                                    }
                                    MenuItem {
                                        text: qsTr("Quick Delete (rm)")
                                        onTriggered: root.deleteRemotePath(modelData.path, true)
                                    }
                                    MenuSeparator {}
                                    MenuItem {
                                        text: qsTr("Permissions: %1").arg(modelData.permissions || qsTr("unknown"))
                                        enabled: false
                                    }
                                    MenuSeparator {}
                                    MenuItem {
                                        text: qsTr("Set 644")
                                        onTriggered: root.changeRemotePermissions(modelData.path, "644")
                                    }
                                    MenuItem {
                                        text: qsTr("Set 755")
                                        onTriggered: root.changeRemotePermissions(modelData.path, "755")
                                    }
                                    MenuItem {
                                        text: qsTr("Set 600")
                                        onTriggered: root.changeRemotePermissions(modelData.path, "600")
                                    }
                                    MenuItem {
                                        text: qsTr("Custom Permissions...")
                                        onTriggered: root.openChmodDialog(modelData.path,
                                                                          modelData.permissions || "")
                                    }
                                }
                            }
                        }

                        ScrollBar.vertical: ScrollBar {}
                    }

                    Label {
                        text: root.remoteLoading ? qsTr("Loading...") : root.remoteError
                        color: "#64748b"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}
