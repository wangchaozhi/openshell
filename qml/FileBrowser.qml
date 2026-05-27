pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "filebrowser" as FileBrowserComponents
import "filebrowser/FileFormat.js" as FileFormat

Rectangle {
    id: root

    property var session: ({})
    property string uiTheme: "dark"
    readonly property bool classic: theme.classic
    property string terminalRemotePath: ""
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
    property bool pendingChmodIsDir: false
    property string pendingUploadPath: ""
    property string pendingRemotePath: ""
    property var remoteMenuEntry: ({})
    property string nameDialogMode: ""
    property string editingRemotePath: ""
    property string editingLocalPath: ""
    property string editingConnectionId: ""
    readonly property string connectionId: session && session.connectionId ? session.connectionId : ""
    readonly property string sessionKey: session && session.id ? session.id : ""
    property string loadedSessionKey: ""
    property var remoteStateBySession: ({})
    property var remoteRequestOwners: ({})
    property var remoteOperationOwners: ({})
    property string draggedLocalPath: ""
    property string dropRemoteTargetPath: ""
    property bool remoteDropActive: false
    property bool syncRemoteWithTerminal: true
    property var transferTasks: []
    property bool transferHistoryLoaded: false
    property var downloadTargetFolders: ({})
    property bool transferPanelOpen: false
    property var localDetachedWindow: null
    property var remoteDetachedWindow: null
    property var localBackStack: []
    property var localForwardStack: []
    property var remoteBackStack: []
    property var remoteForwardStack: []
    // 长按窗格把手后置为 true：四周显示流动虚线，提示可以拖出。
    property bool localDetaching: false
    property bool remoteDetaching: false
    readonly property bool remoteListingLoading: remoteLoading && remoteRequestId.length > 0
    readonly property string sessionStatus: session && session.status ? session.status : ""
    readonly property bool sessionConnected: sessionStatus === "connected"
    readonly property bool sessionProblem: sessionStatus === "disconnected" || sessionStatus === "error"
    property alias theme: browserTheme
    readonly property color pageColor: theme.window
    readonly property color panelColor: theme.panel
    readonly property color surfaceColor: theme.surface
    readonly property color hoverColor: theme.hover
    readonly property color selectedColor: theme.selected
    readonly property color dropHoverColor: theme.dropHover
    readonly property color borderColor: theme.border
    readonly property color mutedBorderColor: theme.borderMuted
    readonly property color primaryTextColor: theme.textPrimary
    readonly property color secondaryTextColor: theme.textSecondary
    readonly property color mutedTextColor: theme.textMuted
    readonly property color headerTextColor: theme.textHeader
    readonly property color activeTextColor: theme.textActive
    readonly property color iconColor: theme.icon
    readonly property color iconAccentColor: theme.iconAccent

    signal sftpStatusChanged(string status, string message)
    signal panesDetachedChanged(bool bothDetached)

    property string localSortColumn: "name"
    property bool localSortAsc: true
    property string remoteSortColumn: "name"
    property bool remoteSortAsc: true

    property var localColumnOrder: ["name", "size", "permissions", "modified"]
    property var remoteColumnOrder: ["name", "size", "owner", "permissions", "modified"]

    property string dragPanel: ""
    property int dragSourceIndex: -1
    property int dragTargetIndex: -1
    property string quickLocatePanel: ""
    property string quickLocatePrefix: ""

    readonly property int columnSpacing: 8

    color: pageColor
    border.color: borderColor

    ThemePalette {
        id: browserTheme
        mode: root.uiTheme
    }

    Component.onCompleted: {
        loadTransferHistory()
        refreshLocal()
        switchRemoteSession()
    }

    onTransferTasksChanged: {
        if (transferHistoryLoaded) {
            saveTransferHistory()
        }
    }

    FileBrowserComponents.FileBrowserDialogs {
        id: dialogs
        anchors.fill: parent
        fileBrowser: root
    }

    Timer {
        id: remoteRequestTimeout
        interval: 12000
        repeat: false
        onTriggered: {
            if (!root.remoteListingLoading) {
                return
            }
            root.forgetRequestOwner(root.remoteRequestId, false)
            root.remoteLoading = false
            root.remoteRequestId = ""
            root.remoteError = qsTr("Remote listing timed out. Try refresh again.")
            root.sftpStatusChanged("error", root.remoteError)
            root.saveCurrentRemoteState()
        }
    }

    Timer {
        id: quickLocateReset
        interval: 900
        repeat: false
        onTriggered: {
            root.quickLocatePanel = ""
            root.quickLocatePrefix = ""
        }
    }

    onSessionKeyChanged: switchRemoteSession()

    function columnTitle(colId) {
        switch (colId) {
        case "name": return qsTr("Name")
        case "size": return qsTr("Size")
        case "owner": return qsTr("User/Group")
        case "permissions": return qsTr("Perm")
        case "modified": return qsTr("Modified")
        }
        return colId
    }

    function columnFixedWidth(colId) { return FileFormat.columnFixedWidth(colId) }

    function columnAlignsRight(colId) { return FileFormat.columnAlignsRight(colId) }

    function entryNameColor(entry) { return FileFormat.entryNameColor(entry) }

    function columnOrderFor(panel) {
        return panel === "local" ? localColumnOrder : remoteColumnOrder
    }

    function setColumnOrderFor(panel, order) {
        if (panel === "local") localColumnOrder = order
        else remoteColumnOrder = order
    }

    function totalFixedWidth(panel) {
        const order = columnOrderFor(panel)
        let sum = 0
        for (let i = 0; i < order.length; i++) sum += columnFixedWidth(order[i])
        return sum
    }

    function columnWidthAt(panel, colId, totalWidth) {
        const fixed = columnFixedWidth(colId)
        if (fixed > 0) return fixed
        const order = columnOrderFor(panel)
        const spacing = Math.max(0, order.length - 1) * columnSpacing
        return Math.max(80, totalWidth - totalFixedWidth(panel) - spacing)
    }

    function buildVisualOrder(panel) {
        const order = columnOrderFor(panel).slice()
        if (dragPanel === panel && dragSourceIndex >= 0 && dragTargetIndex >= 0
                && dragSourceIndex !== dragTargetIndex) {
            const moved = order.splice(dragSourceIndex, 1)[0]
            order.splice(dragTargetIndex, 0, moved)
        }
        return order
    }

    function visualIndexOf(panel, naturalIndex) {
        if (panel !== dragPanel || dragSourceIndex < 0 || dragTargetIndex < 0) return naturalIndex
        if (naturalIndex === dragSourceIndex) return dragTargetIndex
        if (dragSourceIndex < dragTargetIndex) {
            if (naturalIndex > dragSourceIndex && naturalIndex <= dragTargetIndex) return naturalIndex - 1
        } else {
            if (naturalIndex >= dragTargetIndex && naturalIndex < dragSourceIndex) return naturalIndex + 1
        }
        return naturalIndex
    }

    function slotX(panel, visualIndex, totalWidth) {
        const visualOrder = buildVisualOrder(panel)
        let x = 0
        for (let i = 0; i < visualIndex && i < visualOrder.length; i++) {
            x += columnWidthAt(panel, visualOrder[i], totalWidth) + columnSpacing
        }
        return x
    }

    function computeTargetIndex(panel, centerX, totalWidth) {
        const visualOrder = buildVisualOrder(panel)
        let pos = 0
        for (let i = 0; i < visualOrder.length; i++) {
            const w = columnWidthAt(panel, visualOrder[i], totalWidth)
            if (centerX < pos + w / 2) return i
            pos += w + columnSpacing
        }
        return Math.max(0, visualOrder.length - 1)
    }

    function commitColumnDrag() {
        if (dragPanel !== "" && dragSourceIndex >= 0 && dragTargetIndex >= 0
                && dragSourceIndex !== dragTargetIndex) {
            const order = columnOrderFor(dragPanel).slice()
            const moved = order.splice(dragSourceIndex, 1)[0]
            order.splice(dragTargetIndex, 0, moved)
            setColumnOrderFor(dragPanel, order)
        }
        dragPanel = ""
        dragSourceIndex = -1
        dragTargetIndex = -1
    }

    function cancelColumnDrag() {
        dragPanel = ""
        dragSourceIndex = -1
        dragTargetIndex = -1
    }

    function sortEntries(entries, column, asc) { return FileFormat.sortEntries(entries, column, asc) }

    function sortIcon(col, currentCol, asc) { return FileFormat.sortIcon(col, currentCol, asc) }

    function cloneEntries(entries) {
        return entries ? entries.slice() : []
    }

    function currentRemoteState() {
        return {
            connectionId: connectionId,
            remotePath: remotePath,
            remoteEntries: cloneEntries(remoteEntries),
            remoteError: remoteError,
            remoteLoading: remoteLoading,
            remoteRequestId: remoteRequestId,
            remoteOperationRequestId: remoteOperationRequestId,
            remoteSortColumn: remoteSortColumn,
            remoteSortAsc: remoteSortAsc,
            remoteBackStack: remoteBackStack.slice(),
            remoteForwardStack: remoteForwardStack.slice()
        }
    }

    function saveRemoteStateForKey(key, state) {
        if (!key || key.length === 0) {
            return
        }
        const next = Object.assign({}, remoteStateBySession)
        next[key] = state
        remoteStateBySession = next
    }

    function saveCurrentRemoteState() {
        if (loadedSessionKey && loadedSessionKey.length > 0) {
            saveRemoteStateForKey(loadedSessionKey, currentRemoteState())
        }
    }

    function stateForSession(key, connId) {
        if (key && remoteStateBySession[key]) {
            return remoteStateBySession[key]
        }
        return {
            connectionId: connId,
            remotePath: connId && connId.length > 0 ? appController.remoteHomePath(connId) : "",
            remoteEntries: [],
            remoteError: connId && connId.length > 0 ? "" : qsTr("Open a session to enable SFTP browsing."),
            remoteLoading: false,
            remoteRequestId: "",
            remoteOperationRequestId: "",
            remoteSortColumn: "name",
            remoteSortAsc: true,
            remoteBackStack: [],
            remoteForwardStack: []
        }
    }

    function applyRemoteState(state) {
        remotePath = state.remotePath || ""
        remoteEntries = cloneEntries(state.remoteEntries)
        remoteError = state.remoteError || ""
        remoteLoading = state.remoteLoading || false
        remoteRequestId = state.remoteRequestId || ""
        remoteOperationRequestId = state.remoteOperationRequestId || ""
        remoteSortColumn = state.remoteSortColumn || "name"
        remoteSortAsc = state.remoteSortAsc === undefined ? true : state.remoteSortAsc
        remoteBackStack = state.remoteBackStack ? state.remoteBackStack.slice() : []
        remoteForwardStack = state.remoteForwardStack ? state.remoteForwardStack.slice() : []
        if (remoteLoading) {
            remoteRequestTimeout.restart()
        } else {
            remoteRequestTimeout.stop()
        }
    }

    function switchRemoteSession() {
        saveCurrentRemoteState()
        loadedSessionKey = sessionKey
        const state = stateForSession(sessionKey, connectionId)
        applyRemoteState(state)
        if (connectionId === "") {
            return
        }
        if (!state.remoteRequestId && !state.remoteOperationRequestId
                && state.remoteEntries.length === 0 && state.remoteError.length === 0) {
            refreshRemote()
        }
    }

    function rememberRequestOwner(requestId, operation) {
        if (!requestId || requestId.length === 0 || sessionKey.length === 0) {
            return
        }
        const owners = operation ? Object.assign({}, remoteOperationOwners)
                                 : Object.assign({}, remoteRequestOwners)
        owners[requestId] = sessionKey
        if (operation) {
            remoteOperationOwners = owners
        } else {
            remoteRequestOwners = owners
        }
    }

    function forgetRequestOwner(requestId, operation) {
        if (!requestId || requestId.length === 0) {
            return
        }
        const owners = operation ? Object.assign({}, remoteOperationOwners)
                                 : Object.assign({}, remoteRequestOwners)
        delete owners[requestId]
        if (operation) {
            remoteOperationOwners = owners
        } else {
            remoteRequestOwners = owners
        }
    }

    function updateRemoteStateForKey(key, patch) {
        if (!key || key.length === 0) {
            return
        }
        const base = key === sessionKey ? currentRemoteState()
                                        : stateForSession(key, patch.connectionId || "")
        const state = Object.assign({}, base, patch)
        saveRemoteStateForKey(key, state)
        if (key === sessionKey) {
            applyRemoteState(state)
        }
    }

    function requestRemoteForSession(key, connId, path) {
        if (!key || key.length === 0 || !connId || connId.length === 0) {
            return
        }
        root.sftpStatusChanged("connecting", "")
        const requestId = appController.requestRemoteDirectoryEntries(connId, path)
        const owners = Object.assign({}, remoteRequestOwners)
        owners[requestId] = key
        remoteRequestOwners = owners
        updateRemoteStateForKey(key, {
            connectionId: connId,
            remotePath: path,
            remoteError: "",
            remoteLoading: true,
            remoteRequestId: requestId,
            remoteOperationRequestId: ""
        })
        if (key === sessionKey) {
            remoteRequestTimeout.restart()
        }
    }

    function startRemoteOperation(requestId) {
        if (!requestId || requestId.length === 0) {
            return
        }
        rememberRequestOwner(requestId, true)
        remoteOperationRequestId = requestId
        remoteError = ""
        saveCurrentRemoteState()
    }

    function refreshLocal() {
        const raw = appController.localDirectoryEntries(localPath)
        localEntries = sortEntries(raw, localSortColumn, localSortAsc)
    }

    function sortLocal(column) {
        if (localSortColumn === column) {
            localSortAsc = !localSortAsc
        } else {
            localSortColumn = column
            localSortAsc = true
        }
        localEntries = sortEntries(localEntries, localSortColumn, localSortAsc)
    }

    function sortRemote(column) {
        if (remoteSortColumn === column) {
            remoteSortAsc = !remoteSortAsc
        } else {
            remoteSortColumn = column
            remoteSortAsc = true
        }
        remoteEntries = sortEntries(remoteEntries, remoteSortColumn, remoteSortAsc)
        saveCurrentRemoteState()
    }

    function navigateLocal(path, recordHistory) {
        if (!path || path.length === 0) {
            return
        }
        if (path === localPath) {
            refreshLocal()
            return
        }
        if (recordHistory && localPath.length > 0) {
            localBackStack = localBackStack.concat([localPath])
            localForwardStack = []
        }
        localPath = path
        refreshLocal()
    }

    function enterLocal(path) {
        navigateLocal(path, true)
    }

    function localParent() {
        enterLocal(appController.localParentPath(localPath))
    }

    function goLocalBack() {
        if (localBackStack.length === 0) {
            return
        }
        const previous = localBackStack[localBackStack.length - 1]
        localBackStack = localBackStack.slice(0, localBackStack.length - 1)
        if (localPath.length > 0) {
            localForwardStack = localForwardStack.concat([localPath])
        }
        navigateLocal(previous, false)
    }

    function goLocalForward() {
        if (localForwardStack.length === 0) {
            return
        }
        const next = localForwardStack[localForwardStack.length - 1]
        localForwardStack = localForwardStack.slice(0, localForwardStack.length - 1)
        if (localPath.length > 0) {
            localBackStack = localBackStack.concat([localPath])
        }
        navigateLocal(next, false)
    }

    function refreshRemote() {
        if (connectionId === "") {
            remoteEntries = []
            remoteLoading = false
            remoteRequestId = ""
            remoteOperationRequestId = ""
            remoteRequestTimeout.stop()
            remoteError = qsTr("Open a session to enable SFTP browsing.")
            saveCurrentRemoteState()
            return
        }
        requestRemoteForSession(sessionKey, connectionId, remotePath)
    }

    function navigateRemote(path, recordHistory) {
        if (!path || path.length === 0) {
            return
        }
        if (path === remotePath) {
            refreshRemote()
            return
        }
        if (recordHistory && remotePath.length > 0) {
            remoteBackStack = remoteBackStack.concat([remotePath])
            remoteForwardStack = []
        }
        remotePath = path
        refreshRemote()
    }

    function enterRemote(path) {
        navigateRemote(path, true)
    }

    function remoteParent() {
        enterRemote(appController.remoteParentPath(remotePath))
    }

    function goRemoteBack() {
        if (remoteBackStack.length === 0) {
            return
        }
        const previous = remoteBackStack[remoteBackStack.length - 1]
        remoteBackStack = remoteBackStack.slice(0, remoteBackStack.length - 1)
        if (remotePath.length > 0) {
            remoteForwardStack = remoteForwardStack.concat([remotePath])
        }
        navigateRemote(previous, false)
    }

    function goRemoteForward() {
        if (remoteForwardStack.length === 0) {
            return
        }
        const next = remoteForwardStack[remoteForwardStack.length - 1]
        remoteForwardStack = remoteForwardStack.slice(0, remoteForwardStack.length - 1)
        if (remotePath.length > 0) {
            remoteBackStack = remoteBackStack.concat([remotePath])
        }
        navigateRemote(next, false)
    }

    function entryName(entry) {
        return entry && entry.name ? String(entry.name) : ""
    }

    function locateEntryIndex(entries, prefix, startIndex) {
        if (!entries || entries.length === 0 || !prefix || prefix.length === 0) {
            return -1
        }
        const needle = prefix.toLocaleLowerCase()
        const count = entries.length
        for (let i = 0; i < count; ++i) {
            const index = (startIndex + i + count) % count
            const name = entryName(entries[index]).toLocaleLowerCase()
            if (name.indexOf(needle) === 0) {
                return index
            }
        }
        return -1
    }

    // list 由调用方（各面板的 ListView）传入，这样面板拆成独立组件后
    // FileBrowser 不再需要直接持有 localList / remoteList 的 id。
    function quickLocate(panel, text, list) {
        if (!text || text.length !== 1) {
            return false
        }
        const key = text.toLocaleLowerCase()
        if (key < "a" || key > "z") {
            return false
        }

        const entries = panel === "local" ? localEntries : remoteEntries
        const repeatKey = quickLocatePanel === panel
                          && quickLocatePrefix === key
                          && quickLocateReset.running
        const prefix = repeatKey ? key
                                 : (quickLocatePanel === panel && quickLocateReset.running
                                    ? quickLocatePrefix + key
                                    : key)
        const startIndex = repeatKey ? list.currentIndex + 1 : 0
        let match = locateEntryIndex(entries, prefix, startIndex)
        if (match < 0 && prefix.length > 1) {
            match = locateEntryIndex(entries, key, list.currentIndex + 1)
            quickLocatePrefix = key
        } else {
            quickLocatePrefix = prefix
        }
        if (match >= 0) {
            list.currentIndex = match
            list.positionViewAtIndex(match, ListView.Contain)
        }
        quickLocatePanel = panel
        quickLocateReset.restart()
        return match >= 0
    }

    function resolveTerminalRemotePath(path) {
        if (!path || path.length === 0) {
            return ""
        }
        if (path === "~") {
            return appController.remoteHomePath(connectionId)
        }
        if (path.indexOf("~/") === 0) {
            return appController.remoteHomePath(connectionId) + path.substring(1)
        }
        return path.charAt(0) === "/" ? path : ""
    }

    function syncRemotePathFromTerminal(path) {
        if (!syncRemoteWithTerminal || connectionId === "") {
            return
        }
        const resolved = resolveTerminalRemotePath(path)
        if (resolved.length === 0 || resolved === remotePath) {
            return
        }
        enterRemote(resolved)
    }

    function uploadLocalPathTo(path, targetRemoteDirectory) {
        if (connectionId === "" || path.length === 0) {
            return
        }
        pendingUploadPath = path
        const targetPath = targetRemoteDirectory && targetRemoteDirectory.length > 0
                       ? targetRemoteDirectory
                       : remotePath
        startRemoteOperation(appController.requestUploadLocalPath(connectionId,
                                                                  path,
                                                                  targetPath))
    }

    function uploadLocalPath(path) {
        uploadLocalPathTo(path, remotePath)
    }

    function uploadDroppedUrls(urls, targetRemoteDirectory) {
        if (connectionId === "" || !urls || urls.length === 0) {
            return
        }
        for (let i = 0; i < urls.length; ++i) {
            const path = appController.localPathFromUrl(urls[i])
            if (path && path.length > 0) {
                uploadLocalPathTo(path, targetRemoteDirectory)
            }
        }
    }

    function handleRemoteDrop(drop, targetRemoteDirectory) {
        if (connectionId === "") {
            drop.accepted = false
            return
        }
        const targetPath = targetRemoteDirectory && targetRemoteDirectory.length > 0
                       ? targetRemoteDirectory
                       : remotePath
        if (draggedLocalPath.length > 0) {
            uploadLocalPathTo(draggedLocalPath, targetPath)
            drop.accepted = true
        } else if (drop.hasUrls) {
            uploadDroppedUrls(drop.urls, targetPath)
            drop.accepted = true
        } else {
            drop.accepted = false
        }
        draggedLocalPath = ""
        dropRemoteTargetPath = ""
        remoteDropActive = false
    }

    function chooseAndUploadFileTo(targetRemoteDirectory) {
        const path = appController.chooseLocalFile()
        if (path && path.length > 0) {
            uploadLocalPathTo(path, targetRemoteDirectory || remotePath)
        }
    }

    function chooseAndUploadFolderTo(targetRemoteDirectory) {
        const path = appController.chooseLocalFolder()
        if (path && path.length > 0) {
            uploadLocalPathTo(path, targetRemoteDirectory || remotePath)
        }
    }

    function chooseAndUploadFile() {
        chooseAndUploadFileTo(remotePath)
    }

    function chooseAndUploadFolder() {
        chooseAndUploadFolderTo(remotePath)
    }

    function uploadTargetForRemoteMenuEntry() {
        const entry = remoteMenuEntry || ({})
        return entry && entry.isDir && entry.path ? entry.path : remotePath
    }

    function updateDetachedPaneState() {
        panesDetachedChanged(!!localDetachedWindow && !!remoteDetachedWindow)
    }

    function positionDetachedWindow(detachedWindow, sourceItem) {
        if (!detachedWindow || !sourceItem || !sourceItem.mapToItem) {
            return
        }
        const hostWindow = root.Window.window
        if (!hostWindow) {
            return
        }
        const point = sourceItem.mapToItem(null, sourceItem.width / 2, sourceItem.height / 2)
        detachedWindow.x = Math.round(hostWindow.x + point.x - 32)
        detachedWindow.y = Math.round(hostWindow.y + point.y - 24)
    }

    function moveDetachedWindow(detachedWindow, sourceItem, translation) {
        if (!detachedWindow || !sourceItem || !sourceItem.mapToItem) {
            return
        }
        const hostWindow = root.Window.window
        if (!hostWindow) {
            return
        }
        const point = sourceItem.mapToItem(null, sourceItem.width / 2, sourceItem.height / 2)
        detachedWindow.x = Math.round(hostWindow.x + point.x + translation.x - 32)
        detachedWindow.y = Math.round(hostWindow.y + point.y + translation.y - 24)
    }

    // 拖拽过程中把新窗口交给系统窗口管理器，由它跟随仍按住的鼠标移动。
    // 必须等窗口渲染出第一帧后再进入系统移动循环，否则该模态循环会
    // 卡住渲染线程，只剩一个还没画出内容的空框跟着鼠标走。
    function followCursorWithWindow(detachedWindow) {
        if (detachedWindow && detachedWindow.startSystemMove) {
            detachedWindow.pendingSystemMove = true
        }
    }

    function detachLocalPane(sourceItem) {
        if (localDetachedWindow) {
            positionDetachedWindow(localDetachedWindow, sourceItem)
            localDetachedWindow.show()
            localDetachedWindow.raise()
            localDetachedWindow.requestActivate()
            updateDetachedPaneState()
            followCursorWithWindow(localDetachedWindow)
            return
        }
        localDetachedWindow = localDetachedWindowComponent.createObject(root)
        if (localDetachedWindow) {
            positionDetachedWindow(localDetachedWindow, sourceItem)
            localDetachedWindow.show()
            localDetachedWindow.raise()
            localDetachedWindow.requestActivate()
            updateDetachedPaneState()
            followCursorWithWindow(localDetachedWindow)
        }
    }

    function detachRemotePane(sourceItem) {
        if (remoteDetachedWindow) {
            positionDetachedWindow(remoteDetachedWindow, sourceItem)
            remoteDetachedWindow.show()
            remoteDetachedWindow.raise()
            remoteDetachedWindow.requestActivate()
            updateDetachedPaneState()
            followCursorWithWindow(remoteDetachedWindow)
            return
        }
        remoteDetachedWindow = remoteDetachedWindowComponent.createObject(root)
        if (remoteDetachedWindow) {
            positionDetachedWindow(remoteDetachedWindow, sourceItem)
            remoteDetachedWindow.show()
            remoteDetachedWindow.raise()
            remoteDetachedWindow.requestActivate()
            updateDetachedPaneState()
            followCursorWithWindow(remoteDetachedWindow)
        }
    }

    function changeRemotePermissions(path, permissions) {
        if (connectionId === "" || path.length === 0) {
            return
        }
        root.startRemoteOperation(appController.requestRemoteChmod(root.connectionId,
                                                                   path,
                                                                   permissions))
    }

    function openChmodDialog(path, currentPermissions, isDir) {
        if (connectionId === "" || path.length === 0) {
            return
        }
        pendingChmodPath = path
        pendingChmodIsDir = isDir || false
        const parts = path.split("/")
        pendingChmodName = parts.length > 0 && parts[parts.length - 1].length > 0
                         ? parts[parts.length - 1]
                         : path
        dialogs.openChmod(currentPermissions)
    }

    function downloadRemotePath(path) {
        const folder = appController.chooseDownloadFolder()
        if (!folder || folder.length === 0) {
            return
        }
        const requestId = appController.requestRemoteDownload(connectionId, path, folder)
        rememberDownloadTarget(requestId, folder)
        startRemoteOperation(requestId)
    }

    function openRemotePath(path) {
        startRemoteOperation(appController.requestOpenRemotePath(connectionId, path))
    }

    function chooseExternalEditor() {
        const path = appController.chooseExternalTextEditor()
        if (path && path.length > 0) {
            appController.externalTextEditorPath = path
            appController.remoteFileOpenMode = "custom"
        }
    }

    function openRemoteOpenSettings() {
        dialogs.openRemoteOpenSettings()
    }

    function openNameDialog(mode, path, currentName) {
        nameDialogMode = mode
        pendingRemotePath = path || ""
        dialogs.openName(currentName)
    }

    function deleteRemotePath(path, recursive) {
        startRemoteOperation(appController.requestDeleteRemotePath(connectionId, path, recursive))
    }

    function formatFileSize(bytes) { return FileFormat.formatFileSize(bytes) }

    function formatSpeed(bytesPerSecond) { return FileFormat.formatSpeed(bytesPerSecond) }

    function shortPath(path) { return FileFormat.shortPath(path) }

    function transferTasksFor(operation) {
        const rows = []
        for (let i = 0; i < transferTasks.length; ++i) {
            if (transferTasks[i].operation === operation) {
                rows.push(transferTasks[i])
            }
        }
        return rows
    }

    function normalizeSavedTransfer(task) {
        const status = task.status === "running" ? "failed" : (task.status || "done")
        return {
            requestId: task.requestId || ("saved-" + Date.now() + "-" + Math.random()),
            connectionId: task.connectionId || "",
            operation: task.operation === "upload" ? "upload" : "download",
            path: task.path || "",
            localPath: task.localPath || "",
            localDirectory: task.localDirectory || "",
            done: Number(task.done) || 0,
            total: Number(task.total) || 0,
            speed: 0,
            status: status,
            message: task.status === "running" ? qsTr("Interrupted") : (task.message || ""),
            startedAt: Number(task.startedAt) || Date.now(),
            finishedAt: Number(task.finishedAt) || Number(task.updatedAt) || Date.now()
        }
    }

    function loadTransferHistory() {
        const saved = appController.transferHistory()
        const next = []
        for (let i = 0; i < saved.length; ++i) {
            const task = normalizeSavedTransfer(saved[i])
            if ((task.operation === "upload" || task.operation === "download") && task.path.length > 0) {
                next.push(task)
            }
        }
        transferTasks = next.slice(0, 60)
        transferHistoryLoaded = true
    }

    function saveTransferHistory() {
        const saved = []
        for (let i = 0; i < transferTasks.length && saved.length < 60; ++i) {
            const task = transferTasks[i]
            saved.push({
                requestId: task.requestId || "",
                connectionId: task.connectionId || "",
                operation: task.operation || "",
                path: task.path || "",
                localPath: task.localPath || "",
                localDirectory: task.localDirectory || "",
                done: Number(task.done) || 0,
                total: Number(task.total) || 0,
                status: task.status || "done",
                message: task.message || "",
                startedAt: Number(task.startedAt) || Date.now(),
                finishedAt: Number(task.finishedAt) || 0
            })
        }
        appController.saveTransferHistory(saved)
    }

    function rememberDownloadTarget(requestId, folder) {
        if (!requestId || requestId.length === 0 || !folder || folder.length === 0) {
            return
        }
        const targets = Object.assign({}, downloadTargetFolders)
        targets[requestId] = folder
        downloadTargetFolders = targets
    }

    function downloadOpenPath(task) { return FileFormat.downloadOpenPath(task) }

    function openDownloadedFolder(task) {
        const path = downloadOpenPath(task)
        if (path.length > 0) {
            appController.openLocalFolderForPath(path)
        }
    }

    function cancelTransferTask(task) {
        if (!task || task.status !== "running" || !task.requestId) {
            return
        }
        appController.cancelRemoteOperation(task.requestId)
        const next = transferTasks.slice()
        for (let i = 0; i < next.length; ++i) {
            if (next[i].requestId === task.requestId) {
                next[i] = Object.assign({}, next[i], {
                    status: "canceling",
                    message: qsTr("Stopping...")
                })
                break
            }
        }
        transferTasks = next
    }

    function transferPercent(task) { return FileFormat.transferPercent(task) }

    function upsertTransferTask(requestId, connectionId, operation, path, done, total, speed) {
        const next = transferTasks.slice()
        let found = false
        for (let i = 0; i < next.length; ++i) {
            if (next[i].requestId === requestId) {
                next[i] = {
                    requestId: requestId,
                    connectionId: connectionId,
                    operation: operation,
                    path: path,
                    done: done,
                    total: total,
                    speed: speed,
                    status: next[i].status || "running",
                    message: next[i].message || "",
                    localPath: next[i].localPath || "",
                    localDirectory: next[i].localDirectory || "",
                    startedAt: next[i].startedAt || Date.now(),
                    finishedAt: next[i].finishedAt || 0
                }
                found = true
                break
            }
        }
        if (!found) {
            next.unshift({
                requestId: requestId,
                connectionId: connectionId,
                operation: operation,
                path: path,
                done: done,
                total: total,
                speed: speed,
                status: "running",
                message: "",
                localPath: "",
                localDirectory: operation === "download" ? (downloadTargetFolders[requestId] || "") : "",
                startedAt: Date.now(),
                finishedAt: 0
            })
        }
        transferTasks = next.slice(0, 60)
    }

    function finishTransferTask(requestId, ok, message, operation, path, connectionId) {
        const next = transferTasks.slice()
        for (let i = 0; i < next.length; ++i) {
            if (next[i].requestId === requestId) {
                const done = ok && next[i].total > 0 ? next[i].total : next[i].done
                const localPath = ok && next[i].operation === "download" ? (message || next[i].localPath || "") : next[i].localPath
                const canceled = !ok && message === qsTr("Transfer cancelled")
                next[i] = Object.assign({}, next[i], {
                    done: done,
                    speed: ok ? 0 : next[i].speed,
                    status: ok ? "done" : (canceled ? "canceled" : "failed"),
                    message: message || "",
                    localPath: localPath,
                    finishedAt: Date.now()
                })
                transferTasks = next
                return
            }
        }
        if (operation === "upload" || operation === "download") {
            const localPath = ok && operation === "download" ? (message || "") : ""
            next.unshift({
                requestId: requestId,
                connectionId: connectionId || "",
                operation: operation,
                path: path || "",
                done: 0,
                total: 0,
                speed: 0,
                status: ok ? "done" : (!ok && message === qsTr("Transfer cancelled") ? "canceled" : "failed"),
                message: message || "",
                localPath: localPath,
                localDirectory: operation === "download" ? (downloadTargetFolders[requestId] || "") : "",
                startedAt: Date.now(),
                finishedAt: Date.now()
            })
            transferTasks = next.slice(0, 60)
        }
    }

    function clearFinishedTransfers() {
        const next = []
        for (let i = 0; i < transferTasks.length; ++i) {
            if (transferTasks[i].status === "running") {
                next.push(transferTasks[i])
            }
        }
        transferTasks = next
    }

    function permissionsToSymbolic(octal, isDir) { return FileFormat.permissionsToSymbolic(octal, isDir) }

    onTerminalRemotePathChanged: syncRemotePathFromTerminal(terminalRemotePath)
    onSyncRemoteWithTerminalChanged: {
        if (syncRemoteWithTerminal) {
            syncRemotePathFromTerminal(terminalRemotePath)
        }
    }

    Connections {
        target: appController
        function onRemoteDirectoryEntriesReady(requestId, connectionId, path, entries, error) {
            const key = root.remoteRequestOwners[requestId] || ""
            if (key.length === 0) {
                return
            }
            root.forgetRequestOwner(requestId, false)
            const base = key === root.sessionKey ? root.currentRemoteState()
                                                 : root.stateForSession(key, connectionId)
            const sortColumn = base.remoteSortColumn || "name"
            const sortAsc = base.remoteSortAsc === undefined ? true : base.remoteSortAsc
            root.updateRemoteStateForKey(key, {
                connectionId: connectionId,
                remotePath: path,
                remoteEntries: root.sortEntries(entries, sortColumn, sortAsc),
                remoteError: error || "",
                remoteLoading: false,
                remoteRequestId: ""
            })
            if (key === root.sessionKey) {
                remoteRequestTimeout.stop()
                root.sftpStatusChanged(error && error.length > 0 ? "error" : "connected", error || "")
            }
        }

        function onRemoteOperationFinished(requestId, connectionId, operation, path, ok, message) {
            root.finishTransferTask(requestId, ok, message, operation, path, connectionId)
            const key = root.remoteOperationOwners[requestId] || ""
            if (key.length === 0) {
                return
            }
            root.forgetRequestOwner(requestId, true)
            const base = key === root.sessionKey ? root.currentRemoteState()
                                                 : root.stateForSession(key, connectionId)
            root.updateRemoteStateForKey(key, {
                connectionId: connectionId,
                remoteOperationRequestId: "",
                remoteError: ok ? "" : message
            })
            if (ok) {
                if (key === root.sessionKey) {
                    root.sftpStatusChanged("connected", "")
                }
                root.requestRemoteForSession(key, connectionId, base.remotePath || appController.remoteHomePath(connectionId))
            } else if (key === root.sessionKey) {
                root.sftpStatusChanged("error", message || "")
                remoteRequestTimeout.stop()
            }
        }

        function onRemoteOperationProgress(requestId, connectionId, operation, path, bytesDone, bytesTotal, speedBytesPerSec) {
            if (operation !== "upload" && operation !== "download") {
                return
            }
            root.upsertTransferTask(requestId,
                                    connectionId,
                                    operation,
                                    path,
                                    bytesDone,
                                    bytesTotal,
                                    speedBytesPerSec)
        }

        function onRemoteFileReadyForInternalEditor(connectionId, remotePath, localPath, text, error) {
            if (connectionId !== root.connectionId || error.length > 0) {
                return
            }
            root.editingConnectionId = connectionId
            root.editingRemotePath = remotePath
            root.editingLocalPath = localPath
            dialogs.openInternalEditor(text)
        }
    }

    Component {
        id: localDetachedWindowComponent
        FileBrowserComponents.DetachedLocalWindow { fileBrowser: root }
    }

    Component {
        id: remoteDetachedWindowComponent
        FileBrowserComponents.DetachedRemoteWindow { fileBrowser: root }
    }

    SplitView {
        anchors.fill: parent
        anchors.margins: 4
        orientation: Qt.Horizontal

        handle: Rectangle {
            implicitWidth: 8
            implicitHeight: 8
                color: SplitHandle.pressed ? "#38bdf8"
                  : SplitHandle.hovered ? "#2563eb" : root.borderColor

            Rectangle {
                width: 2
                height: Math.min(44, parent.height - 12)
                radius: 1
                anchors.centerIn: parent
                color: SplitHandle.pressed || SplitHandle.hovered ? "#bfdbfe" : root.iconColor
            }
        }

        FileBrowserComponents.LocalPane {
            visible: !root.localDetachedWindow
            SplitView.preferredWidth: root.localDetachedWindow ? 0 : Math.max(320, (root.width - 16) / 2)
            SplitView.minimumWidth: root.localDetachedWindow ? 0 : 260
            SplitView.fillWidth: !!root.remoteDetachedWindow
            SplitView.fillHeight: true
            fileBrowser: root
        }

        FileBrowserComponents.RemotePane {
            visible: !root.remoteDetachedWindow
            SplitView.preferredWidth: root.remoteDetachedWindow ? 0 : Math.max(320, (root.width - 16) / 2)
            SplitView.minimumWidth: root.remoteDetachedWindow ? 0 : 320
            SplitView.fillWidth: true
            SplitView.fillHeight: true
            fileBrowser: root
        }
    }
}
