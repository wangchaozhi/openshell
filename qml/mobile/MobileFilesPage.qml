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
                chmodField.text = contextMenu.targetPermissions
                                  && contextMenu.targetPermissions.length > 0
                                  ? contextMenu.targetPermissions
                                  : (contextMenu.targetIsDir ? "755" : "644")
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
            const local = appController.localPathFromUrl(selectedFile.toString())
            if (!local || local.length === 0) {
                root.errorMessage = qsTr("Unable to resolve picked file path")
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
        modal: true
        title: qsTr("Rename")
        standardButtons: Dialog.Ok | Dialog.Cancel

        TextField {
            id: renameField
            width: 280
            placeholderText: qsTr("New name")
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
        modal: true
        title: qsTr("Delete")
        standardButtons: Dialog.Yes | Dialog.No

        property string targetPath: ""
        property bool targetIsDir: false

        Label {
            text: qsTr("Delete %1?").arg(deleteDialog.targetPath)
            wrapMode: Text.WordWrap
            width: 280
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
        modal: true
        title: qsTr("Permissions")
        standardButtons: Dialog.Ok | Dialog.Cancel

        property string targetPath: ""

        ColumnLayout {
            width: 280
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: chmodDialog.targetPath
                color: "#cbd5f5"
                font.pixelSize: 11
                elide: Text.ElideMiddle
            }

            TextField {
                id: chmodField
                Layout.fillWidth: true
                placeholderText: qsTr("Octal (e.g. 755)")
                inputMethodHints: Qt.ImhDigitsOnly
                validator: RegularExpressionValidator { regularExpression: /^[0-7]{3,4}$/ }
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Examples: 755 (rwxr-xr-x), 644 (rw-r--r--)")
                color: "#64748b"
                font.pixelSize: 10
                wrapMode: Text.WordWrap
            }
        }

        onAccepted: {
            if (chmodField.text.length === 0) return
            appController.requestRemoteChmod(root.connectionId,
                                             chmodDialog.targetPath,
                                             chmodField.text)
        }
    }

    Dialog {
        id: newFolderDialog
        anchors.centerIn: parent
        modal: true
        title: qsTr("New Folder")
        standardButtons: Dialog.Ok | Dialog.Cancel

        TextField {
            id: newFolderField
            width: 280
            placeholderText: qsTr("Folder name")
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
