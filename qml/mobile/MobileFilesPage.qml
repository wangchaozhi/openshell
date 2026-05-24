import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts

Rectangle {
    id: root

    property bool hasSession: false
    property string connectionId: ""

    property string currentPath: ""
    property var entries: []
    property bool loading: false
    property string pendingRequestId: ""
    property string errorMessage: ""
    property string pendingOp: ""        // "delete" | "rename" | "create" | "upload" — refresh-pending
    property string renameTargetPath: ""

    // 上传/下载进度——监听 remoteOperationProgress 同步更新
    property string activeTransferRequestId: ""
    property string activeTransferOp: ""     // "upload" / "download"
    property string activeTransferLabel: ""
    property real activeTransferBytesDone: 0
    property real activeTransferBytesTotal: 0
    property real activeTransferSpeed: 0

    color: "#0b1220"

    function loadDirectory(path) {
        if (!hasSession || connectionId.length === 0) {
            return
        }
        errorMessage = ""
        loading = true
        pendingRequestId = appController.requestRemoteDirectoryEntries(connectionId, path)
    }

    function refresh() {
        loadDirectory(currentPath.length > 0 ? currentPath : appController.remoteHomePath(connectionId))
    }

    function navigateUp() {
        if (currentPath.length === 0) {
            return
        }
        const parent = appController.remoteParentPath(currentPath)
        if (parent === currentPath) {
            return
        }
        loadDirectory(parent)
    }

    function navigateHome() {
        loadDirectory(appController.remoteHomePath(connectionId))
    }

    function formatSize(bytes) {
        const n = Number(bytes)
        if (!isFinite(n) || n < 0) return ""
        if (n < 1024) return n + " B"
        if (n < 1024 * 1024) return (n / 1024).toFixed(1) + " KB"
        if (n < 1024 * 1024 * 1024) return (n / 1024 / 1024).toFixed(1) + " MB"
        return (n / 1024 / 1024 / 1024).toFixed(1) + " GB"
    }

    function joinPath(dir, name) {
        if (dir === "/") return "/" + name
        if (dir.endsWith("/")) return dir + name
        return dir + "/" + name
    }

    function transferProgress() {
        if (activeTransferBytesTotal <= 0) return 0
        return Math.min(1, activeTransferBytesDone / activeTransferBytesTotal)
    }

    function formatSpeed(bytesPerSec) {
        if (!bytesPerSec || bytesPerSec < 0) return ""
        return formatSize(bytesPerSec) + "/s"
    }

    onHasSessionChanged: {
        if (hasSession && currentPath.length === 0 && connectionId.length > 0) {
            navigateHome()
        } else if (!hasSession) {
            entries = []
            currentPath = ""
            errorMessage = ""
        }
    }

    onConnectionIdChanged: {
        currentPath = ""
        entries = []
        errorMessage = ""
        if (hasSession && connectionId.length > 0) {
            navigateHome()
        }
    }

    Connections {
        target: appController

        function onRemoteDirectoryEntriesReady(requestId, connId, path, entries, error) {
            if (requestId !== root.pendingRequestId) {
                return
            }
            root.pendingRequestId = ""
            root.loading = false
            if (error && error.length > 0) {
                root.errorMessage = error
                return
            }
            root.currentPath = path
            // 对前端友好排序：先目录后文件，名字字母序。
            const sorted = entries.slice().sort(function (a, b) {
                const ad = a && a.isDir ? 1 : 0
                const bd = b && b.isDir ? 1 : 0
                if (ad !== bd) return bd - ad
                return String(a && a.name || "").localeCompare(String(b && b.name || ""))
            })
            root.entries = sorted
        }

        function onRemoteOperationFinished(requestId, connId, operation, path, ok, message) {
            if (requestId === root.activeTransferRequestId) {
                root.activeTransferRequestId = ""
                root.activeTransferOp = ""
                root.activeTransferLabel = ""
                root.activeTransferBytesDone = 0
                root.activeTransferBytesTotal = 0
                root.activeTransferSpeed = 0
            }
            if (!ok) {
                root.errorMessage = message && message.length > 0
                                    ? message
                                    : qsTr("Operation failed: %1").arg(operation)
                return
            }
            if (operation === "delete" || operation === "rename"
                    || operation === "create" || operation === "upload"
                    || operation === "chmod") {
                root.refresh()
            }
        }

        function onRemoteOperationProgress(requestId, connId, operation, path,
                                           bytesDone, bytesTotal, speedBytesPerSec) {
            if (requestId !== root.activeTransferRequestId) {
                return
            }
            root.activeTransferOp = operation
            root.activeTransferBytesDone = bytesDone
            root.activeTransferBytesTotal = bytesTotal
            root.activeTransferSpeed = speedBytesPerSec
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        // 标题 + 导航按钮
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: qsTr("Files")
                color: "#f8fafc"
                font.pixelSize: 22
                font.bold: true
            }

            Button {
                Layout.preferredHeight: 38
                enabled: root.hasSession && !root.loading
                text: qsTr("Home")
                onClicked: root.navigateHome()
            }

            Button {
                Layout.preferredHeight: 38
                enabled: root.hasSession && !root.loading
                text: qsTr("Refresh")
                onClicked: root.refresh()
            }
        }

        // 路径栏 + 返回上级
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            visible: root.hasSession
            radius: 6
            color: "#0f172a"
            border.color: "#1e293b"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 8

                Button {
                    Layout.preferredHeight: 30
                    leftPadding: 10
                    rightPadding: 10
                    enabled: root.currentPath.length > 0 && root.currentPath !== "/" && !root.loading
                    text: qsTr("Up")
                    onClicked: root.navigateUp()
                }

                Label {
                    Layout.fillWidth: true
                    text: root.currentPath.length > 0 ? root.currentPath : qsTr("(no path)")
                    color: "#cbd5f5"
                    font.pixelSize: 12
                    elide: Text.ElideMiddle
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        // 上传/下载进度条
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            visible: root.activeTransferRequestId.length > 0
            radius: 6
            color: "#0f172a"
            border.color: "#1e293b"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        text: {
                            const verb = root.activeTransferOp === "upload" ? qsTr("Uploading")
                                       : root.activeTransferOp === "download" ? qsTr("Downloading")
                                       : qsTr("Transferring")
                            return root.activeTransferLabel.length > 0
                                   ? verb + " " + root.activeTransferLabel
                                   : verb
                        }
                        color: "#e2e8f0"
                        font.pixelSize: 12
                        elide: Text.ElideMiddle
                    }

                    Label {
                        text: {
                            if (root.activeTransferBytesTotal <= 0) {
                                return root.formatSize(root.activeTransferBytesDone)
                            }
                            const pct = Math.round(root.transferProgress() * 100)
                            return pct + "%   "
                                 + root.formatSize(root.activeTransferBytesDone)
                                 + " / " + root.formatSize(root.activeTransferBytesTotal)
                                 + (root.activeTransferSpeed > 0
                                    ? "   " + root.formatSpeed(root.activeTransferSpeed) : "")
                        }
                        color: "#94a3b8"
                        font.pixelSize: 11
                    }
                }

                ProgressBar {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 6
                    from: 0
                    to: 1
                    value: root.transferProgress()
                    indeterminate: root.activeTransferBytesTotal <= 0
                }
            }
        }

        // 错误条
        Label {
            Layout.fillWidth: true
            visible: root.errorMessage.length > 0
            text: root.errorMessage
            color: "#fca5a5"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        // 主体：未连接占位 / 列表 / 加载中
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 8
            color: "#111827"
            border.color: "#1e293b"

            // 未开 session 占位
            Label {
                anchors.centerIn: parent
                visible: !root.hasSession
                width: Math.min(parent.width - 32, 320)
                text: qsTr("Open a session first")
                color: "#e2e8f0"
                font.pixelSize: 15
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            BusyIndicator {
                anchors.centerIn: parent
                visible: root.loading
                running: visible
            }

            ListView {
                id: list
                anchors.fill: parent
                anchors.margins: 6
                clip: true
                spacing: 4
                visible: root.hasSession && !root.loading
                model: root.entries

                delegate: Rectangle {
                    id: row
                    required property var modelData

                    readonly property string entryName: row.modelData && row.modelData.name ? row.modelData.name : ""
                    readonly property bool isDir: !!(row.modelData && row.modelData.isDir)
                    readonly property string entryPath: root.joinPath(root.currentPath, entryName)

                    width: ListView.view.width
                    height: 56
                    radius: 6
                    color: rowMouse.pressed ? "#1e3a8a" : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 10

                        Rectangle {
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 36
                            radius: 18
                            color: row.isDir ? "#1e3a8a" : "#0f172a"
                            border.color: row.isDir ? "#60a5fa" : "#1e293b"

                            Label {
                                anchors.centerIn: parent
                                text: row.isDir ? "/" : "•"
                                color: row.isDir ? "#bfdbfe" : "#94a3b8"
                                font.pixelSize: 16
                                font.bold: true
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Label {
                                Layout.fillWidth: true
                                text: row.entryName
                                color: "#f8fafc"
                                font.pixelSize: 14
                                font.bold: row.isDir
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                visible: !row.isDir
                                       || (row.modelData && row.modelData.modified)
                                text: row.isDir
                                    ? (row.modelData && row.modelData.modified ? String(row.modelData.modified) : "")
                                    : root.formatSize(row.modelData ? row.modelData.size : 0)
                                      + ((row.modelData && row.modelData.modified)
                                         ? "   " + row.modelData.modified
                                         : "")
                                color: "#94a3b8"
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                        }
                    }

                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        onClicked: {
                            if (row.isDir) {
                                root.loadDirectory(row.entryPath)
                            } else {
                                contextMenu.targetPath = row.entryPath
                                contextMenu.targetPermissions = row.modelData
                                                                && row.modelData.permissions
                                                                ? String(row.modelData.permissions)
                                                                : ""
                                contextMenu.targetIsDir = false
                                contextMenu.popup()
                            }
                        }
                        onPressAndHold: {
                            contextMenu.targetPath = row.entryPath
                            contextMenu.targetPermissions = row.modelData
                                                            && row.modelData.permissions
                                                            ? String(row.modelData.permissions)
                                                            : ""
                            contextMenu.targetIsDir = row.isDir
                            contextMenu.popup()
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: list.count === 0 && !root.loading
                    text: qsTr("Empty directory")
                    color: "#64748b"
                    font.pixelSize: 13
                }
            }
        }
    }

    // 长按 / 文件点击弹出的菜单
    Menu {
        id: contextMenu

        property string targetPath: ""
        property string targetPermissions: ""
        property bool targetIsDir: false

        width: 180

        MenuItem {
            width: parent.width
            height: 36
            text: qsTr("Copy Path")
            enabled: contextMenu.targetPath.length > 0
            onTriggered: {
                appController.copyTextToClipboard(contextMenu.targetPath)
                root.errorMessage = ""
            }
        }
        MenuSeparator {}
        MenuItem {
            width: parent.width
            height: 36
            text: qsTr("Rename")
            enabled: contextMenu.targetPath.length > 0
            onTriggered: {
                root.renameTargetPath = contextMenu.targetPath
                const slash = contextMenu.targetPath.lastIndexOf("/")
                renameField.text = slash >= 0
                                   ? contextMenu.targetPath.substring(slash + 1)
                                   : contextMenu.targetPath
                renameDialog.open()
            }
        }
        MenuItem {
            width: parent.width
            height: 36
            text: qsTr("Permissions")
            enabled: contextMenu.targetPath.length > 0
            onTriggered: {
                chmodDialog.targetPath = contextMenu.targetPath
                chmodDialog.targetIsDir = contextMenu.targetIsDir
                chmodDialog.applyOctal(contextMenu.targetPermissions
                                       && contextMenu.targetPermissions.length > 0
                                       ? contextMenu.targetPermissions
                                       : (contextMenu.targetIsDir ? "755" : "644"))
                chmodDialog.open()
            }
        }
        MenuSeparator {}
        MenuItem {
            width: parent.width
            height: 36
            text: qsTr("Delete")
            enabled: contextMenu.targetPath.length > 0
            onTriggered: {
                deleteDialog.targetPath = contextMenu.targetPath
                deleteDialog.targetIsDir = contextMenu.targetIsDir
                deleteDialog.open()
            }
        }
    }

    // 右下角的浮动操作按钮：上传 + 新建文件夹
    ColumnLayout {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 18
        spacing: 8

        Button {
            Layout.alignment: Qt.AlignRight
            visible: root.hasSession && !root.loading && root.currentPath.length > 0
            text: qsTr("Upload")
            leftPadding: 16
            rightPadding: 16
            highlighted: true
            onClicked: uploadPicker.open()
        }

        Button {
            Layout.alignment: Qt.AlignRight
            visible: root.hasSession && !root.loading
            text: qsTr("New Folder")
            leftPadding: 14
            rightPadding: 14
            onClicked: {
                newFolderField.text = ""
                newFolderDialog.open()
            }
        }
    }

    // 通过系统 picker 选择要上传的本地文件。Android 走 Storage Access
    // Framework，iOS 走 UIDocumentPicker；都不需要单独申请存储权限。
    FileDialog {
        id: uploadPicker
        title: qsTr("Pick a file to upload")
        fileMode: FileDialog.OpenFile
        onAccepted: {
            // Android SAF returns content:// URIs that QFile can read but
            // QFileInfo / QDir cannot walk — stage them into the app sandbox
            // first so the existing upload pipeline keeps working.
            const local = appController.stageUrlForUpload(selectedFile.toString())
            if (!local || local.length === 0) {
                root.errorMessage = appController.lastError.length > 0
                    ? appController.lastError
                    : qsTr("Unable to stage picked file")
                return
            }
            root.pendingOp = "upload"
            const slash = local.lastIndexOf("/")
            root.activeTransferLabel = slash >= 0 ? local.substring(slash + 1) : local
            root.activeTransferOp = "upload"
            root.activeTransferBytesDone = 0
            root.activeTransferBytesTotal = 0
            root.activeTransferSpeed = 0
            root.activeTransferRequestId = appController.requestUploadLocalPath(
                root.connectionId, local, root.currentPath)
        }
    }

    Dialog {
        id: renameDialog
        anchors.centerIn: parent
        width: Math.min(parent.width - 64, 360)
        modal: true
        title: qsTr("Rename")
        standardButtons: Dialog.Ok | Dialog.Cancel

        ColumnLayout {
            width: parent.width

            TextField {
                id: renameField
                Layout.fillWidth: true
                placeholderText: qsTr("New name")
            }
        }

        onAccepted: {
            if (renameField.text.length === 0) return
            root.pendingOp = "rename"
            appController.requestRenameRemotePath(root.connectionId,
                                                  root.renameTargetPath,
                                                  renameField.text)
        }
    }

    Dialog {
        id: deleteDialog
        anchors.centerIn: parent
        width: Math.min(parent.width - 64, 360)
        modal: true
        title: qsTr("Delete")
        standardButtons: Dialog.Yes | Dialog.No

        property string targetPath: ""
        property bool targetIsDir: false

        Label {
            text: qsTr("Delete %1?").arg(deleteDialog.targetPath)
            wrapMode: Text.WordWrap
            width: parent.width
        }

        onAccepted: {
            root.pendingOp = "delete"
            appController.requestDeleteRemotePath(root.connectionId,
                                                  deleteDialog.targetPath,
                                                  deleteDialog.targetIsDir)
        }
    }

    Dialog {
        id: chmodDialog
        anchors.centerIn: parent
        width: Math.min(parent.width - 64, 360)
        modal: true
        title: qsTr("Permissions")
        standardButtons: Dialog.Ok | Dialog.Cancel

        property string targetPath: ""
        property bool targetIsDir: false

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
            const prefix = chmodDialog.targetIsDir ? "d" : "-"
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
            octalLabel.text = octalText()
        }

        ColumnLayout {
            width: parent.width
            spacing: 8

            RowLayout {
                Layout.fillWidth: true

                Label {
                    Layout.fillWidth: true
                    text: chmodDialog.targetPath
                    color: "#cbd5f5"
                    font.pixelSize: 11
                    elide: Text.ElideMiddle
                }

                Label {
                    id: octalLabel
                    text: "755"
                    color: "#38bdf8"
                    font.pixelSize: 13
                    font.bold: true
                }
            }

            Label {
                id: symbolicLabel
                Layout.fillWidth: true
                text: "----------"
                color: "#94a3b8"
                font.family: "Courier New"
                font.pixelSize: 13
                font.bold: true
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 4
                columnSpacing: 2
                rowSpacing: 0

                Item { Layout.preferredWidth: 48 }
                Label { Layout.alignment: Qt.AlignHCenter; text: qsTr("Read"); color: "#cbd5f5"; font.pixelSize: 11 }
                Label { Layout.alignment: Qt.AlignHCenter; text: qsTr("Write"); color: "#cbd5f5"; font.pixelSize: 11 }
                Label { Layout.alignment: Qt.AlignHCenter; text: qsTr("Exec"); color: "#cbd5f5"; font.pixelSize: 11 }

                Label { text: qsTr("Owner"); color: "#cbd5f5"; font.pixelSize: 12 }
                CheckBox { id: ownerRead; Layout.alignment: Qt.AlignHCenter; onCheckedChanged: chmodDialog.updateSymbolic() }
                CheckBox { id: ownerWrite; Layout.alignment: Qt.AlignHCenter; onCheckedChanged: chmodDialog.updateSymbolic() }
                CheckBox { id: ownerExec; Layout.alignment: Qt.AlignHCenter; onCheckedChanged: chmodDialog.updateSymbolic() }

                Label { text: qsTr("Group"); color: "#cbd5f5"; font.pixelSize: 12 }
                CheckBox { id: groupRead; Layout.alignment: Qt.AlignHCenter; onCheckedChanged: chmodDialog.updateSymbolic() }
                CheckBox { id: groupWrite; Layout.alignment: Qt.AlignHCenter; onCheckedChanged: chmodDialog.updateSymbolic() }
                CheckBox { id: groupExec; Layout.alignment: Qt.AlignHCenter; onCheckedChanged: chmodDialog.updateSymbolic() }

                Label { text: qsTr("Other"); color: "#cbd5f5"; font.pixelSize: 12 }
                CheckBox { id: otherRead; Layout.alignment: Qt.AlignHCenter; onCheckedChanged: chmodDialog.updateSymbolic() }
                CheckBox { id: otherWrite; Layout.alignment: Qt.AlignHCenter; onCheckedChanged: chmodDialog.updateSymbolic() }
                CheckBox { id: otherExec; Layout.alignment: Qt.AlignHCenter; onCheckedChanged: chmodDialog.updateSymbolic() }
            }
        }

        onAccepted: {
            appController.requestRemoteChmod(root.connectionId,
                                             chmodDialog.targetPath,
                                             chmodDialog.octalText())
        }
    }

    Dialog {
        id: newFolderDialog
        anchors.centerIn: parent
        width: Math.min(parent.width - 64, 360)
        modal: true
        title: qsTr("New Folder")
        standardButtons: Dialog.Ok | Dialog.Cancel

        ColumnLayout {
            width: parent.width

            TextField {
                id: newFolderField
                Layout.fillWidth: true
                placeholderText: qsTr("Folder name")
            }
        }

        onAccepted: {
            if (newFolderField.text.length === 0) return
            root.pendingOp = "create"
            appController.requestCreateRemotePath(root.connectionId,
                                                  root.currentPath,
                                                  newFolderField.text,
                                                  true)
        }
    }
}
