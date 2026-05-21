import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "filebrowser" as FileBrowserComponents

Rectangle {
    id: root

    property var session: ({})
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
    // 长按窗格把手后置为 true：四周显示流动虚线，提示可以拖出。
    property bool localDetaching: false
    property bool remoteDetaching: false
    readonly property bool remoteListingLoading: remoteLoading && remoteRequestId.length > 0
    readonly property string sessionStatus: session && session.status ? session.status : ""
    readonly property bool sessionConnected: sessionStatus === "connected"
    readonly property bool sessionProblem: sessionStatus === "disconnected" || sessionStatus === "error"

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

    component SyncIcon: Item {
        property bool active: true
        property bool connected: true
        property bool problem: false
        readonly property color strokeColor: problem ? "#f87171"
                                           : connected ? (active ? "#93c5fd" : "#64748b")
                                           : "#fbbf24"
        readonly property color accentColor: problem ? "#ef4444"
                                           : connected ? (active ? "#38bdf8" : "#64748b")
                                           : "#f59e0b"
        readonly property color dotColor: problem ? "#fecaca"
                                        : connected ? (active ? "#dbeafe" : "#94a3b8")
                                        : "#fde68a"

        implicitWidth: 15
        implicitHeight: 15

        Rectangle {
            x: 2
            y: 2
            width: 11
            height: 11
            radius: 3
            color: "transparent"
            border.color: parent.strokeColor
            border.width: 2
        }
        Rectangle {
            x: 7
            y: 1
            width: 5
            height: 5
            rotation: 45
            color: parent.accentColor
        }
        Rectangle {
            x: 3
            y: 9
            width: 5
            height: 5
            rotation: 45
            color: parent.accentColor
        }
        Rectangle {
            x: 2
            y: 5
            width: 5
            height: 5
            color: "#020617"
        }
        Rectangle {
            x: 8
            y: 5
            width: 5
            height: 5
            color: "#020617"
        }
        Rectangle {
            x: 6
            y: 6
            width: 3
            height: 3
            radius: 1
            color: parent.dotColor
        }
    }

    component SettingsIcon: Item {
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
            x: 6
            y: 0
            width: 3
            height: 15
            radius: 1
            color: "#93c5fd"
        }
        Rectangle {
            x: 0
            y: 6
            width: 15
            height: 3
            radius: 1
            color: "#93c5fd"
        }
        Rectangle {
            x: 5
            y: 5
            width: 5
            height: 5
            radius: 3
            color: "#020617"
            border.color: "#dbeafe"
            border.width: 1
        }
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

    function columnFixedWidth(colId) {
        switch (colId) {
        case "size": return 78
        case "owner": return 100
        case "permissions": return 78
        case "modified": return 118
        }
        return 0
    }

    function columnAlignsRight(colId) {
        return colId === "size" || colId === "permissions"
    }

    function entryNameColor(entry) {
        if (!entry) {
            return "#d1d5db"
        }
        const name = String(entry.name || "")
        if (entry.isDir) {
            return "#3b82f6"
        }
        if (name.match(/\.(tar|tgz|arc|arj|taz|lha|lz4|lzh|lzma|tlz|txz|tzo|t7z|zip|z|dz|gz|lrz|lz|lzo|xz|zst|tzst|bz2|bz|tbz|tbz2|tz|deb|rpm|jar|war|ear|sar|rar|alz|ace|zoo|cpio|7z|rz|cab|wim|swm|dwm|esd)$/i)) {
            return "#ef4444"
        }
        if (name.match(/\.(jpg|jpeg|mjpg|mjpeg|gif|bmp|pbm|pgm|ppm|tga|xbm|xpm|tif|tiff|png|svg|svgz|mng|pcx|mov|mpg|mpeg|m2v|mkv|webm|webp|ogm|mp4|m4v|mp4v|vob|qt|nuv|wmv|asf|rm|rmvb|flc|avi|fli|flv|gl|dl|xcf|xwd|yuv|cgm|emf|ogv|ogx)$/i)) {
            return "#d946ef"
        }
        if (name.match(/\.(aac|au|flac|m4a|mid|midi|mka|mp3|mpc|ogg|ra|wav|oga|opus|spx|xspf)$/i)) {
            return "#06b6d4"
        }
        if (permissionsToSymbolic(String(entry.permissions || ""), false).indexOf("x") >= 0) {
            return "#22c55e"
        }
        return "#d1d5db"
    }

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

    function sortEntries(entries, column, asc) {
        const arr = entries.slice()
        arr.sort(function(a, b) {
            // 鐩綍濮嬬粓缃《
            if (a.isDir !== b.isDir) {
                return a.isDir ? -1 : 1
            }
            let va = a[column] || ""
            let vb = b[column] || ""
            if (column === "size") {
                const na = parseInt(va) || 0
                const nb = parseInt(vb) || 0
                return asc ? na - nb : nb - na
            }
            va = String(va).toLowerCase()
            vb = String(vb).toLowerCase()
            if (va < vb) return asc ? -1 : 1
            if (va > vb) return asc ? 1 : -1
            return 0
        })
        return arr
    }

    function sortIcon(col, currentCol, asc) {
        if (col !== currentCol) return ""
        return asc ? " ^" : " v"
    }

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
            remoteSortAsc: remoteSortAsc
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
            remoteSortAsc: true
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

    function enterLocal(path) {
        localPath = path
        refreshLocal()
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

    function enterRemote(path) {
        remotePath = path
        refreshRemote()
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

    function quickLocate(panel, text) {
        if (!text || text.length !== 1) {
            return false
        }
        const key = text.toLocaleLowerCase()
        if (key < "a" || key > "z") {
            return false
        }

        const list = panel === "local" ? localList : remoteList
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

    function formatFileSize(bytes) {
        const strVal = String(bytes).trim()
        if (!strVal || strVal === "0" || strVal === "--" || strVal === "undefined") {
            return "--"
        }
        const size = Number(strVal)
        if (isNaN(size) || size < 0) {
            return strVal
        }
        if (size === 0) {
            return "0 B"
        }
        const units = ["B", "KB", "MB", "GB", "TB"]
        let unitIndex = 0
        let value = size
        while (value >= 1024 && unitIndex < units.length - 1) {
            value /= 1024
            unitIndex++
        }
        if (unitIndex === 0) {
            return Math.floor(value) + " " + units[unitIndex]
        } else {
            return value.toFixed(1) + " " + units[unitIndex]
        }
    }

    function formatSpeed(bytesPerSecond) {
        const speed = Number(bytesPerSecond) || 0
        return formatFileSize(speed) + "/s"
    }

    function shortPath(path) {
        if (!path || path.length === 0) {
            return ""
        }
        const normalized = String(path).replace(/\\/g, "/")
        const parts = normalized.split("/")
        return parts.length > 0 && parts[parts.length - 1].length > 0
               ? parts[parts.length - 1]
               : normalized
    }

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

    function downloadOpenPath(task) {
        if (!task || task.operation !== "download" || task.status !== "done") {
            return ""
        }
        return task.localPath && task.localPath.length > 0 ? task.localPath : (task.message || "")
    }

    function openDownloadedFolder(task) {
        const path = downloadOpenPath(task)
        if (path.length > 0) {
            appController.openLocalFolderForPath(path)
        }
    }

    function transferPercent(task) {
        if (!task || !task.total || task.total <= 0) {
            return 0
        }
        return Math.max(0, Math.min(100, Math.round(task.done * 100 / task.total)))
    }

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
                next[i] = Object.assign({}, next[i], {
                    done: done,
                    speed: ok ? 0 : next[i].speed,
                    status: ok ? "done" : "failed",
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
                status: ok ? "done" : "failed",
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

    function permissionsToSymbolic(octal, isDir) {
        if (!octal || octal.length === 0) {
            return isDir ? "d---------" : "----------"
        }
        const normalized = octal.slice(-3)
        const owner = parseInt(normalized.charAt(0))
        const group = parseInt(normalized.charAt(1))
        const other = parseInt(normalized.charAt(2))

        const toRwx = (val) => (val & 4 ? "r" : "-") + (val & 2 ? "w" : "-") + (val & 1 ? "x" : "-")
        const prefix = isDir ? "d" : "-"
        return prefix + toRwx(owner) + toRwx(group) + toRwx(other)
    }

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

        Window {
            id: localWindow
            property bool pendingSystemMove: false

            onFrameSwapped: {
                if (pendingSystemMove) {
                    pendingSystemMove = false
                    startSystemMove()
                }
            }

            width: 720
            height: 520
            minimumWidth: 420
            minimumHeight: 300
            visible: false
            title: qsTr("Local") + " - " + root.localPath
            color: "#020617"

            Rectangle {
                id: localWindowContent
                anchors.fill: parent
                color: "#020617"

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
                            contentItem: ParentIcon { anchors.centerIn: parent }
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
                            contentItem: RefreshIcon { anchors.centerIn: parent }
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
                        Item {
                            id: detachedLocalHeaderInner
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            Repeater {
                                model: root.localColumnOrder
                                delegate: FileBrowserComponents.HeaderCell {
                                    required property string modelData
                                    required property int index
                                    panelName: "local"
                                    fileBrowser: root
                                    colId: modelData
                                    naturalIndex: index
                                    parentWidth: detachedLocalHeaderInner.width
                                }
                            }
                        }
                    }

                    ListView {
                        id: detachedLocalList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: root.localEntries
                        reuseItems: true
                        cacheBuffer: 900

                        delegate: Rectangle {
                            id: detachedLocalRow
                            required property var modelData
                            required property int index
                            width: detachedLocalList.width
                            height: 26
                            color: detachedLocalMouse.containsMouse ? "#111827" : "#020617"

                            Item {
                                id: detachedLocalRowInner
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                Repeater {
                                    model: root.localColumnOrder
                                    delegate: FileBrowserComponents.DataCell {
                                        required property string modelData
                                        required property int index
                                        panelName: "local"
                                        fileBrowser: root
                                        colId: modelData
                                        naturalIndex: index
                                        parentWidth: detachedLocalRowInner.width
                                        entry: detachedLocalRow.modelData
                                    }
                                }
                            }

                            MouseArea {
                                id: detachedLocalMouse
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                hoverEnabled: true
                                onDoubleClicked: {
                                    if (detachedLocalRow.modelData.isDir) {
                                        root.enterLocal(detachedLocalRow.modelData.path)
                                    }
                                }
                                onClicked: function(mouse) {
                                    if (mouse.button === Qt.RightButton) {
                                        detachedLocalMenu.path = detachedLocalRow.modelData.path
                                        detachedLocalMenu.popup()
                                    }
                                }
                            }
                        }

                        Menu {
                            id: detachedLocalMenu
                            property string path: ""
                            MenuItem {
                                text: qsTr("Upload to Remote")
                                enabled: root.connectionId !== "" && detachedLocalMenu.path.length > 0
                                onTriggered: root.uploadLocalPath(detachedLocalMenu.path)
                            }
                        }

                        ScrollBar.vertical: ScrollBar {}
                    }
                }
            }

            onClosing: function(close) {
                close.accepted = true
                destroy()
            }
            Component.onDestruction: {
                root.localDetachedWindow = null
                root.updateDetachedPaneState()
            }
        }
    }

    Component {
        id: remoteDetachedWindowComponent

        Window {
            id: remoteWindow
            property bool pendingSystemMove: false

            onFrameSwapped: {
                if (pendingSystemMove) {
                    pendingSystemMove = false
                    startSystemMove()
                }
            }

            width: 760
            height: 520
            minimumWidth: 460
            minimumHeight: 300
            visible: false
            title: qsTr("Remote") + " - " + root.remotePath
            color: "#020617"

            Rectangle {
                id: remoteWindowContent
                anchors.fill: parent
                color: "#020617"

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
                        Item {
                            id: detachedRemoteHeaderInner
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            Repeater {
                                model: root.remoteColumnOrder
                                delegate: FileBrowserComponents.HeaderCell {
                                    required property string modelData
                                    required property int index
                                    panelName: "remote"
                                    fileBrowser: root
                                    colId: modelData
                                    naturalIndex: index
                                    parentWidth: detachedRemoteHeaderInner.width
                                }
                            }
                        }
                    }

                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: root.remoteListingLoading || (root.remoteError.length > 0 && root.remoteEntries.length === 0) ? 1 : 0

                        ListView {
                            id: detachedRemoteList
                            clip: true
                            model: root.remoteEntries
                            reuseItems: true
                            cacheBuffer: 900

                            delegate: Rectangle {
                                id: detachedRemoteRow
                                required property var modelData
                                required property int index
                                width: detachedRemoteList.width
                                height: 26
                                color: detachedRemoteMouse.containsMouse ? "#111827" : "#020617"

                                Item {
                                    id: detachedRemoteRowInner
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    Repeater {
                                        model: root.remoteColumnOrder
                                        delegate: FileBrowserComponents.DataCell {
                                            required property string modelData
                                            required property int index
                                            panelName: "remote"
                                            fileBrowser: root
                                            colId: modelData
                                            naturalIndex: index
                                            parentWidth: detachedRemoteRowInner.width
                                            entry: detachedRemoteRow.modelData
                                        }
                                    }
                                }

                                MouseArea {
                                    id: detachedRemoteMouse
                                    anchors.fill: parent
                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                    hoverEnabled: true
                                    onDoubleClicked: {
                                        if (detachedRemoteRow.modelData.isDir) {
                                            root.enterRemote(detachedRemoteRow.modelData.path)
                                        } else {
                                            root.openRemotePath(detachedRemoteRow.modelData.path)
                                        }
                                    }
                                    onClicked: function(mouse) {
                                        if (mouse.button === Qt.RightButton) {
                                            root.remoteMenuEntry = detachedRemoteRow.modelData
                                            detachedRemoteMenu.popup()
                                        }
                                    }
                                }
                            }

                            Menu {
                                id: detachedRemoteMenu
                                readonly property var entry: root.remoteMenuEntry || ({})
                                readonly property bool hasEntry: entry && entry.path
                                readonly property bool isDir: hasEntry && entry.isDir
                                MenuItem {
                                    text: qsTr("Download")
                                    enabled: detachedRemoteMenu.hasEntry
                                    onTriggered: root.downloadRemotePath(detachedRemoteMenu.entry.path)
                                }
                                Menu {
                                    title: qsTr("Upload...")
                                    enabled: detachedRemoteMenu.hasEntry && root.connectionId !== ""
                                    MenuItem {
                                        text: qsTr("File")
                                        onTriggered: root.chooseAndUploadFileTo(root.uploadTargetForRemoteMenuEntry())
                                    }
                                    MenuItem {
                                        text: qsTr("Folder")
                                        onTriggered: root.chooseAndUploadFolderTo(root.uploadTargetForRemoteMenuEntry())
                                    }
                                }
                                MenuSeparator {}
                                MenuItem {
                                    text: qsTr("Rename")
                                    enabled: detachedRemoteMenu.hasEntry
                                    onTriggered: root.openNameDialog("rename",
                                                                     detachedRemoteMenu.entry.path,
                                                                     detachedRemoteMenu.entry.name)
                                }
                                MenuItem {
                                    text: qsTr("Delete")
                                    enabled: detachedRemoteMenu.hasEntry
                                    onTriggered: root.deleteRemotePath(detachedRemoteMenu.entry.path, false)
                                }
                            }

                            ScrollBar.vertical: ScrollBar {}
                        }

                        Label {
                            text: root.remoteListingLoading ? qsTr("Loading...") : root.remoteError
                            color: "#64748b"
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }

            onClosing: function(close) {
                close.accepted = true
                destroy()
            }
            Component.onDestruction: {
                root.remoteDetachedWindow = null
                root.updateDetachedPaneState()
            }
        }
    }

    SplitView {
        anchors.fill: parent
        anchors.margins: 4
        orientation: Qt.Horizontal

        handle: Rectangle {
            implicitWidth: 8
            implicitHeight: 8
            color: SplitHandle.pressed ? "#38bdf8"
                  : SplitHandle.hovered ? "#2563eb" : "#1e293b"

            Rectangle {
                width: 2
                height: Math.min(44, parent.height - 12)
                radius: 1
                anchors.centerIn: parent
                color: SplitHandle.pressed || SplitHandle.hovered ? "#bfdbfe" : "#475569"
            }
        }

        Rectangle {
            visible: !root.localDetachedWindow
            SplitView.preferredWidth: root.localDetachedWindow ? 0 : Math.max(320, (root.width - 16) / 2)
            SplitView.minimumWidth: root.localDetachedWindow ? 0 : 260
            SplitView.fillWidth: !!root.remoteDetachedWindow
            SplitView.fillHeight: true
            color: "#020617"
            radius: 4

            MarchingAntsBorder {
                anchors.fill: parent
                z: 50
                cornerRadius: parent.radius
                active: root.localDetaching
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        id: localTitleLabel
                        property bool detachHoldReady: false
                        property bool detachStarted: false
                        text: qsTr("Local")
                        color: "#cbd5f5"
                        font.bold: true
                        font.pixelSize: 12

                        function resetDetachIfIdle() {
                            if (!localTitleDrag.active && !localTitleLabel.detachStarted) {
                                localTitleArmTimer.stop()
                                localTitleLabel.detachHoldReady = false
                                root.localDetaching = false
                            }
                        }

                        // 按住一小会儿即“就位”，期间允许移动，不像 longPressed
                        // 那样一动就取消，于是按下后可以顺势拖出。
                        Timer {
                            id: localTitleArmTimer
                            interval: 250
                            onTriggered: {
                                localTitleLabel.detachHoldReady = true
                                root.localDetaching = true
                            }
                        }

                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            onPressedChanged: {
                                if (pressed) {
                                    localTitleLabel.detachHoldReady = false
                                    localTitleLabel.detachStarted = false
                                    localTitleArmTimer.restart()
                                } else {
                                    Qt.callLater(localTitleLabel.resetDetachIfIdle)
                                }
                            }
                        }
                        DragHandler {
                            id: localTitleDrag
                            target: null
                            dragThreshold: 0
                            onActiveChanged: {
                                if (!active) {
                                    localTitleArmTimer.stop()
                                    localTitleLabel.detachHoldReady = false
                                    localTitleLabel.detachStarted = false
                                    root.localDetaching = false
                                }
                            }
                            onTranslationChanged: {
                                if (!active || !localTitleLabel.detachHoldReady
                                        || localTitleLabel.detachStarted) {
                                    return
                                }
                                // 长按就位后，开始拖动才真正拆出窗口；
                                // 拆出后由系统窗口管理器接管，窗口跟随鼠标。
                                if (Math.hypot(translation.x, translation.y) <= 6) {
                                    return
                                }
                                localTitleLabel.detachStarted = true
                                root.detachLocalPane(localTitleLabel)
                            }
                        }
                    }

                    ToolButton {
                        id: localDetachButton
                        property bool detachHoldReady: false
                        property bool detachStarted: false
                        implicitWidth: 30
                        implicitHeight: 24
                        text: "[]"
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Detach Window")

                        function resetDetachIfIdle() {
                            if (!localDetachDrag.active && !localDetachButton.detachStarted) {
                                localDetachArmTimer.stop()
                                localDetachButton.detachHoldReady = false
                                root.localDetaching = false
                            }
                        }

                        // 按住一小会儿即“就位”，期间允许移动，不像 longPressed
                        // 那样一动就取消，于是按下后可以顺势拖出。
                        Timer {
                            id: localDetachArmTimer
                            interval: 250
                            onTriggered: {
                                localDetachButton.detachHoldReady = true
                                root.localDetaching = true
                            }
                        }

                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            onPressedChanged: {
                                if (pressed) {
                                    localDetachButton.detachHoldReady = false
                                    localDetachButton.detachStarted = false
                                    localDetachArmTimer.restart()
                                } else {
                                    Qt.callLater(localDetachButton.resetDetachIfIdle)
                                }
                            }
                        }
                        DragHandler {
                            id: localDetachDrag
                            target: null
                            dragThreshold: 0
                            onActiveChanged: {
                                if (!active) {
                                    localDetachArmTimer.stop()
                                    localDetachButton.detachHoldReady = false
                                    localDetachButton.detachStarted = false
                                    root.localDetaching = false
                                }
                            }
                            onTranslationChanged: {
                                if (!active || !localDetachButton.detachHoldReady
                                        || localDetachButton.detachStarted) {
                                    return
                                }
                                if (Math.hypot(translation.x, translation.y) <= 6) {
                                    return
                                }
                                localDetachButton.detachStarted = true
                                root.detachLocalPane(localDetachButton)
                            }
                        }
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

                    Item {
                        id: localHeaderInner
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8

                        Repeater {
                            model: root.localColumnOrder
                            delegate: FileBrowserComponents.HeaderCell {
                                required property string modelData
                                required property int index
                                panelName: "local"
                                fileBrowser: root
                                colId: modelData
                                naturalIndex: index
                                parentWidth: localHeaderInner.width
                            }
                        }
                    }
                }

                ListView {
                    id: localList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: root.localEntries
                    reuseItems: true
                    cacheBuffer: 600
                    activeFocusOnTab: true
                    highlightFollowsCurrentItem: true
                    highlight: Rectangle {
                        color: "#172554"
                    }

                    Keys.onPressed: function(event) {
                        event.accepted = root.quickLocate("local", event.text)
                    }

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.RightButton
                        propagateComposedEvents: true
                        z: 20
                        onClicked: function(mouse) {
                            localList.forceActiveFocus()
                            if (localList.indexAt(mouse.x, mouse.y + localList.contentY) < 0) {
                                localBlankMenu.popup()
                            } else {
                                mouse.accepted = false
                            }
                        }
                    }

                    Menu {
                        id: localBlankMenu
                        MenuItem {
                            text: qsTr("Refresh")
                            onTriggered: root.refreshLocal()
                        }
                        MenuSeparator {}
                        Menu {
                            title: qsTr("Upload to Remote")
                            enabled: root.connectionId !== ""
                            MenuItem {
                                text: qsTr("File")
                                onTriggered: root.chooseAndUploadFile()
                            }
                            MenuItem {
                                text: qsTr("Folder")
                                onTriggered: root.chooseAndUploadFolder()
                            }
                        }
                    }

                    delegate: Rectangle {
                        id: localRow
                        required property var modelData
                        required property int index

                        width: localList.width
                        height: 26
                        color: localList.currentIndex === index ? "#172554"
                              : mouseArea.containsMouse ? "#111827" : "#020617"

                        Drag.active: localDragHandler.active
                        Drag.dragType: Drag.Automatic
                        Drag.supportedActions: Qt.CopyAction
                        Drag.mimeData: {
                            "text/uri-list": "file:///" + localRow.modelData.path.replace(/\\/g, "/")
                        }
                        Drag.hotSpot.x: 10
                        Drag.hotSpot.y: height / 2

                        DragHandler {
                            id: localDragHandler
                            target: null
                            acceptedButtons: Qt.LeftButton
                            onActiveChanged: {
                                root.draggedLocalPath = active ? localRow.modelData.path : ""
                            }
                        }

                        Item {
                            id: localRowInner
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8

                            Repeater {
                                model: root.localColumnOrder
                                delegate: FileBrowserComponents.DataCell {
                                    required property string modelData
                                    required property int index
                                    panelName: "local"
                                    fileBrowser: root
                                    colId: modelData
                                    naturalIndex: index
                                    parentWidth: localRowInner.width
                                    entry: localRow.modelData
                                }
                            }
                        }

                        MouseArea {
                            id: mouseArea
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            hoverEnabled: true
                            onPressed: {
                                root.draggedLocalPath = localRow.modelData.path
                            }
                            onReleased: {
                                root.draggedLocalPath = ""
                            }
                            onCanceled: {
                                root.draggedLocalPath = ""
                            }
                            onClicked: function(mouse) {
                                localList.currentIndex = localRow.index
                                localList.forceActiveFocus()
                                if (mouse.button === Qt.RightButton) {
                                    localItemMenu.popup()
                                }
                            }
                            onDoubleClicked: {
                                if (localRow.modelData.isDir) {
                                    root.enterLocal(localRow.modelData.path)
                                }
                            }

                            Menu {
                                id: localItemMenu
                                MenuItem {
                                    text: qsTr("Upload to Remote")
                                    enabled: root.connectionId !== ""
                                    onTriggered: root.uploadLocalPath(localRow.modelData.path)
                                }
                            }
                        }
                    }

                    ScrollBar.vertical: ScrollBar {}
                }
            }
        }

        Rectangle {
            visible: !root.remoteDetachedWindow
            SplitView.preferredWidth: root.remoteDetachedWindow ? 0 : Math.max(320, (root.width - 16) / 2)
            SplitView.minimumWidth: root.remoteDetachedWindow ? 0 : 320
            SplitView.fillWidth: true
            SplitView.fillHeight: true
            color: "#020617"
            radius: 4

            MarchingAntsBorder {
                anchors.fill: parent
                z: 50
                cornerRadius: parent.radius
                active: root.remoteDetaching
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        id: remoteTitleLabel
                        property bool detachHoldReady: false
                        property bool detachStarted: false
                        text: qsTr("Remote")
                        color: "#cbd5f5"
                        font.bold: true
                        font.pixelSize: 12

                        function resetDetachIfIdle() {
                            if (!remoteTitleDrag.active && !remoteTitleLabel.detachStarted) {
                                remoteTitleArmTimer.stop()
                                remoteTitleLabel.detachHoldReady = false
                                root.remoteDetaching = false
                            }
                        }

                        // 按住一小会儿即“就位”，期间允许移动，不像 longPressed
                        // 那样一动就取消，于是按下后可以顺势拖出。
                        Timer {
                            id: remoteTitleArmTimer
                            interval: 250
                            onTriggered: {
                                remoteTitleLabel.detachHoldReady = true
                                root.remoteDetaching = true
                            }
                        }

                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            onPressedChanged: {
                                if (pressed) {
                                    remoteTitleLabel.detachHoldReady = false
                                    remoteTitleLabel.detachStarted = false
                                    remoteTitleArmTimer.restart()
                                } else {
                                    Qt.callLater(remoteTitleLabel.resetDetachIfIdle)
                                }
                            }
                        }
                        DragHandler {
                            id: remoteTitleDrag
                            target: null
                            dragThreshold: 0
                            onActiveChanged: {
                                if (!active) {
                                    remoteTitleArmTimer.stop()
                                    remoteTitleLabel.detachHoldReady = false
                                    remoteTitleLabel.detachStarted = false
                                    root.remoteDetaching = false
                                }
                            }
                            onTranslationChanged: {
                                if (!active || !remoteTitleLabel.detachHoldReady
                                        || remoteTitleLabel.detachStarted) {
                                    return
                                }
                                // 长按就位后，开始拖动才真正拆出窗口；
                                // 拆出后由系统窗口管理器接管，窗口跟随鼠标。
                                if (Math.hypot(translation.x, translation.y) <= 6) {
                                    return
                                }
                                remoteTitleLabel.detachStarted = true
                                root.detachRemotePane(remoteTitleLabel)
                            }
                        }
                    }

                    ToolButton {
                        id: remoteDetachButton
                        property bool detachHoldReady: false
                        property bool detachStarted: false
                        implicitWidth: 30
                        implicitHeight: 24
                        text: "[]"
                        enabled: root.connectionId !== ""
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Detach Window")

                        function resetDetachIfIdle() {
                            if (!remoteDetachDrag.active && !remoteDetachButton.detachStarted) {
                                remoteDetachArmTimer.stop()
                                remoteDetachButton.detachHoldReady = false
                                root.remoteDetaching = false
                            }
                        }

                        // 按住一小会儿即“就位”，期间允许移动，不像 longPressed
                        // 那样一动就取消，于是按下后可以顺势拖出。
                        Timer {
                            id: remoteDetachArmTimer
                            interval: 250
                            onTriggered: {
                                remoteDetachButton.detachHoldReady = true
                                root.remoteDetaching = true
                            }
                        }

                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            onPressedChanged: {
                                if (pressed) {
                                    remoteDetachButton.detachHoldReady = false
                                    remoteDetachButton.detachStarted = false
                                    remoteDetachArmTimer.restart()
                                } else {
                                    Qt.callLater(remoteDetachButton.resetDetachIfIdle)
                                }
                            }
                        }
                        DragHandler {
                            id: remoteDetachDrag
                            target: null
                            dragThreshold: 0
                            onActiveChanged: {
                                if (!active) {
                                    remoteDetachArmTimer.stop()
                                    remoteDetachButton.detachHoldReady = false
                                    remoteDetachButton.detachStarted = false
                                    root.remoteDetaching = false
                                }
                            }
                            onTranslationChanged: {
                                if (!active || !remoteDetachButton.detachHoldReady
                                        || remoteDetachButton.detachStarted) {
                                    return
                                }
                                if (Math.hypot(translation.x, translation.y) <= 6) {
                                    return
                                }
                                remoteDetachButton.detachStarted = true
                                root.detachRemotePane(remoteDetachButton)
                            }
                        }
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

                    ToolButton {
                        implicitWidth: 30
                        implicitHeight: 24
                        enabled: root.connectionId !== ""
                        checkable: true
                        checked: root.syncRemoteWithTerminal
                        onClicked: root.syncRemoteWithTerminal = checked
                        background: Rectangle {
                            color: root.sessionProblem ? "#2a1014"
                                  : !root.sessionConnected ? "#241b0a"
                                  : parent.checked ? "#0f2742" : "#111827"
                            border.color: root.sessionProblem ? "#ef4444"
                                          : !root.sessionConnected ? "#f59e0b"
                                          : parent.checked ? "#38bdf8" : "#334155"
                            radius: 3
                        }
                        contentItem: SyncIcon {
                            anchors.centerIn: parent
                            active: root.syncRemoteWithTerminal
                            connected: root.sessionConnected
                            problem: root.sessionProblem
                            opacity: parent.enabled ? 1 : 0.35
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: checked ? qsTr("Sync with terminal folder: on")
                                              : qsTr("Sync with terminal folder: off")
                    }

                    ToolButton {
                        implicitWidth: 30
                        implicitHeight: 24
                        contentItem: SettingsIcon {
                            anchors.centerIn: parent
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Remote file open settings")
                        onClicked: root.openRemoteOpenSettings()
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    ToolButton {
                        implicitWidth: 46
                        implicitHeight: 24
                        text: transferTasks.length > 0 ? String(transferTasks.length) : ""
                        enabled: transferTasks.length > 0
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Transfer tasks")
                        onClicked: root.transferPanelOpen = !root.transferPanelOpen
                        background: Rectangle {
                            color: parent.enabled ? "#111827" : "#020617"
                            border.color: root.transferPanelOpen ? "#38bdf8" : "#334155"
                            radius: 3
                        }
                        contentItem: Row {
                            spacing: 4
                            anchors.centerIn: parent
                            Rectangle {
                                width: 12
                                height: 10
                                radius: 2
                                color: root.transferTasks.length > 0 ? "#60a5fa" : "#475569"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Label {
                                text: root.transferTasks.length > 0 ? String(root.transferTasks.length) : ""
                                color: "#dbeafe"
                                font.pixelSize: 11
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
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

                    Item {
                        id: remoteHeaderInner
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8

                        Repeater {
                            model: root.remoteColumnOrder
                            delegate: FileBrowserComponents.HeaderCell {
                                required property string modelData
                                required property int index
                                panelName: "remote"
                                fileBrowser: root
                                colId: modelData
                                naturalIndex: index
                                parentWidth: remoteHeaderInner.width
                            }
                        }
                    }
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: root.remoteListingLoading || (root.remoteError.length > 0 && root.remoteEntries.length === 0) ? 1 : 0

                    ListView {
                        id: remoteList
                        clip: true
                        model: root.remoteEntries
                        reuseItems: true
                        cacheBuffer: 900
                        activeFocusOnTab: true
                        highlightFollowsCurrentItem: true
                        highlight: Rectangle {
                            color: "#172554"
                        }

                        Keys.onPressed: function(event) {
                            event.accepted = root.quickLocate("remote", event.text)
                        }

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.RightButton
                            propagateComposedEvents: true
                            z: 20
                            onClicked: function(mouse) {
                                remoteList.forceActiveFocus()
                                if (remoteList.indexAt(mouse.x, mouse.y + remoteList.contentY) < 0) {
                                    remoteBlankMenu.popup()
                                } else {
                                    mouse.accepted = false
                                }
                            }
                        }

                        Menu {
                            id: remoteBlankMenu
                            MenuItem {
                                text: qsTr("Refresh")
                                enabled: root.connectionId !== ""
                                onTriggered: root.refreshRemote()
                            }
                            MenuSeparator {}
                            Menu {
                                title: qsTr("Upload...")
                                enabled: root.connectionId !== ""
                                MenuItem {
                                    text: qsTr("File")
                                    onTriggered: root.chooseAndUploadFile()
                                }
                                MenuItem {
                                    text: qsTr("Folder")
                                    onTriggered: root.chooseAndUploadFolder()
                                }
                            }
                            MenuSeparator {}
                            Menu {
                                title: qsTr("New")
                                enabled: root.connectionId !== ""
                                MenuItem {
                                    text: qsTr("File")
                                    onTriggered: root.openNameDialog("newFile", "", "new-file")
                                }
                                MenuItem {
                                    text: qsTr("Folder")
                                    onTriggered: root.openNameDialog("newDir", "", "new-folder")
                                }
                            }
                        }

                        Menu {
                            id: remoteItemMenu
                            readonly property var entry: root.remoteMenuEntry || ({})
                            readonly property bool hasEntry: entry && entry.path
                            readonly property bool isDir: hasEntry && entry.isDir

                            MenuItem {
                                text: qsTr("Refresh")
                                onTriggered: root.refreshRemote()
                            }
                            MenuSeparator {}
                            MenuItem {
                                text: qsTr("Open")
                                enabled: remoteItemMenu.hasEntry && !remoteItemMenu.isDir
                                onTriggered: root.openRemotePath(remoteItemMenu.entry.path)
                            }
                            Menu {
                                title: qsTr("Open With")
                                enabled: remoteItemMenu.hasEntry && !remoteItemMenu.isDir
                                MenuItem {
                                    text: qsTr("System default app")
                                    onTriggered: {
                                        appController.remoteFileOpenMode = "system"
                                        root.openRemotePath(remoteItemMenu.entry.path)
                                    }
                                }
                                MenuItem {
                                    text: qsTr("Specified text editor")
                                    onTriggered: {
                                        appController.remoteFileOpenMode = "custom"
                                        root.openRemotePath(remoteItemMenu.entry.path)
                                    }
                                }
                                MenuItem {
                                    text: qsTr("Built-in editor")
                                    onTriggered: {
                                        appController.remoteFileOpenMode = "internal"
                                        root.openRemotePath(remoteItemMenu.entry.path)
                                    }
                                }
                            }
                            Menu {
                                title: qsTr("Select Text Editor")
                                MenuItem {
                                    text: appController.externalTextEditorPath && appController.externalTextEditorPath.length > 0
                                          ? appController.externalTextEditorPath
                                          : qsTr("Browse...")
                                    onTriggered: root.chooseExternalEditor()
                                }
                                MenuItem {
                                    text: qsTr("Open settings")
                                    onTriggered: root.openRemoteOpenSettings()
                                }
                            }
                            MenuSeparator {}
                            MenuItem {
                                text: qsTr("Copy Path")
                                enabled: remoteItemMenu.hasEntry
                                onTriggered: appController.copyTextToClipboard(remoteItemMenu.entry.path)
                            }
                            MenuSeparator {}
                            MenuItem {
                                text: qsTr("Download")
                                enabled: remoteItemMenu.hasEntry
                                onTriggered: root.downloadRemotePath(remoteItemMenu.entry.path)
                            }
                            Menu {
                                title: qsTr("Upload...")
                                enabled: remoteItemMenu.hasEntry && root.connectionId !== ""
                                MenuItem {
                                    text: qsTr("File")
                                    onTriggered: root.chooseAndUploadFileTo(root.uploadTargetForRemoteMenuEntry())
                                }
                                MenuItem {
                                    text: qsTr("Folder")
                                    onTriggered: root.chooseAndUploadFolderTo(root.uploadTargetForRemoteMenuEntry())
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
                                enabled: remoteItemMenu.hasEntry
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
                                enabled: remoteItemMenu.hasEntry
                                onTriggered: root.openNameDialog("rename",
                                                                 remoteItemMenu.entry.path,
                                                                 remoteItemMenu.entry.name)
                            }
                            MenuItem {
                                text: qsTr("Delete")
                                enabled: remoteItemMenu.hasEntry
                                onTriggered: root.deleteRemotePath(remoteItemMenu.entry.path, false)
                            }
                            MenuItem {
                                text: qsTr("Quick Delete (rm)")
                                enabled: remoteItemMenu.hasEntry
                                onTriggered: root.deleteRemotePath(remoteItemMenu.entry.path, true)
                            }
                            MenuSeparator {}
                            MenuItem {
                                text: qsTr("Permissions (%1)").arg(remoteItemMenu.entry.permissions || qsTr("?"))
                                enabled: remoteItemMenu.hasEntry
                                onTriggered: root.openChmodDialog(remoteItemMenu.entry.path,
                                                                  remoteItemMenu.entry.permissions || "",
                                                                  remoteItemMenu.isDir)
                            }
                        }

                        delegate: Rectangle {
                            id: remoteRow
                            required property var modelData
                            required property int index

                            width: remoteList.width
                            height: 26
                            color: remoteRowDropArea.containsDrag ? "#1e3a5f"
                                  : remoteList.currentIndex === index ? "#172554"
                                  : remoteMouseArea.containsMouse ? "#111827" : "#020617"

                            Item {
                                id: remoteRowInner
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8

                                Repeater {
                                    model: root.remoteColumnOrder
                                    delegate: FileBrowserComponents.DataCell {
                                        required property string modelData
                                        required property int index
                                        panelName: "remote"
                                        fileBrowser: root
                                        colId: modelData
                                        naturalIndex: index
                                        parentWidth: remoteRowInner.width
                                        entry: remoteRow.modelData
                                    }
                                }
                            }

                            MouseArea {
                                id: remoteMouseArea
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                hoverEnabled: true
                                onClicked: function(mouse) {
                                    remoteList.currentIndex = remoteRow.index
                                    remoteList.forceActiveFocus()
                                    if (mouse.button === Qt.RightButton) {
                                        root.remoteMenuEntry = remoteRow.modelData
                                        remoteItemMenu.popup()
                                    }
                                }
                                onDoubleClicked: {
                                    if (remoteRow.modelData.isDir) {
                                        root.enterRemote(remoteRow.modelData.path)
                                    } else {
                                        root.openRemotePath(remoteRow.modelData.path)
                                    }
                                }
                            }

                            DropArea {
                                id: remoteRowDropArea
                                anchors.fill: parent
                                enabled: root.connectionId !== "" && remoteRow.modelData.isDir
                                keys: [ "text/uri-list" ]
                                onEntered: function(drag) {
                                    root.remoteDropActive = true
                                    root.dropRemoteTargetPath = remoteRow.modelData.path
                                    drag.accepted = true
                                }
                                onExited: {
                                    root.dropRemoteTargetPath = ""
                                }
                                onDropped: function(drop) {
                                    root.handleRemoteDrop(drop, remoteRow.modelData.path)
                                }
                            }
                        }

                        ScrollBar.vertical: ScrollBar {}
                    }

                    Label {
                        text: root.remoteListingLoading ? qsTr("Loading...") : root.remoteError
                        color: "#64748b"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        wrapMode: Text.WordWrap
                    }
                }
            }

            FileBrowserComponents.TransferPanel {
                fileBrowser: root
            }

            DropArea {
                id: remotePanelDropArea
                anchors.fill: parent
                anchors.topMargin: 58
                enabled: root.connectionId !== ""
                keys: [ "text/uri-list" ]
                onEntered: function(drag) {
                    root.remoteDropActive = true
                    root.dropRemoteTargetPath = root.remotePath
                    drag.accepted = true
                }
                onExited: {
                    root.remoteDropActive = false
                    root.dropRemoteTargetPath = ""
                }
                onDropped: function(drop) {
                    root.handleRemoteDrop(drop, root.remotePath)
                }
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: 8
                visible: root.remoteDropActive
                color: "#0ea5e9"
                opacity: 0.12
                border.color: "#38bdf8"
                border.width: 2
                radius: 4
                z: 20

                Label {
                    anchors.centerIn: parent
                    text: root.dropRemoteTargetPath.length > 0
                          ? qsTr("Upload to %1").arg(root.dropRemoteTargetPath)
                          : qsTr("Upload to Remote")
                    color: "#bfdbfe"
                    font.pixelSize: 13
                    font.bold: true
                    elide: Text.ElideMiddle
                    width: parent.width - 40
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }
}
