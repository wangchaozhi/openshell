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
    property bool pendingChmodIsDir: false
    property string pendingUploadPath: ""
    property string pendingRemotePath: ""
    property string nameDialogMode: ""
    readonly property string connectionId: session && session.connectionId ? session.connectionId : ""
    readonly property string sessionKey: session && session.id ? session.id : ""
    property string loadedSessionKey: ""
    property var remoteStateBySession: ({})
    property var remoteRequestOwners: ({})
    property var remoteOperationOwners: ({})
    property string draggedLocalPath: ""
    property string dropRemoteTargetPath: ""
    property bool remoteDropActive: false
    property var transferTasks: []
    property bool transferPanelOpen: false
    readonly property bool remoteListingLoading: remoteLoading && remoteRequestId.length > 0

    property string localSortColumn: "name"
    property bool localSortAsc: true
    property string remoteSortColumn: "name"
    property bool remoteSortAsc: true

    property var localColumnOrder: ["name", "size", "permissions", "modified"]
    property var remoteColumnOrder: ["name", "size", "owner", "permissions", "modified"]

    property string dragPanel: ""
    property int dragSourceIndex: -1
    property int dragTargetIndex: -1

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

    component FileTypeIcon: Item {
        property bool isDir: false
        property color accentColor: isDir ? "#3b82f6" : "#94a3b8"

        implicitWidth: 16
        implicitHeight: 14

        Rectangle {
            visible: isDir
            x: 1
            y: 2
            width: 7
            height: 3
            radius: 1
            color: accentColor
        }
        Rectangle {
            visible: isDir
            x: 1
            y: 5
            width: 14
            height: 9
            radius: 2
            color: accentColor
            opacity: 0.88
        }
        Rectangle {
            visible: !isDir
            x: 3
            y: 1
            width: 10
            height: 13
            radius: 1
            color: accentColor
        }
        Rectangle {
            visible: !isDir
            x: 10
            y: 1
            width: 3
            height: 3
            color: Qt.lighter(accentColor, 1.35)
        }
    }

    component BroomIcon: Item {
        implicitWidth: 15
        implicitHeight: 15

        Rectangle {
            x: 9
            y: 1
            width: 2
            height: 9
            radius: 1
            rotation: 38
            color: "#cbd5e1"
        }
        Rectangle {
            x: 4
            y: 8
            width: 8
            height: 5
            radius: 1
            rotation: 38
            color: "#facc15"
        }
        Repeater {
            model: 3
            Rectangle {
                x: 3 + index * 3
                y: 11
                width: 1
                height: 3
                rotation: 38
                color: "#fef3c7"
            }
        }
    }

    component HeaderCell: Item {
        id: headerCell

        property string panelName: "local"
        property string colId: ""
        property int naturalIndex: 0
        property real parentWidth: 0

        readonly property bool isDragged: root.dragPanel === panelName && root.dragSourceIndex === naturalIndex
        readonly property int currentVisualIndex: isDragged ? root.dragTargetIndex
                                                            : root.visualIndexOf(panelName, naturalIndex)
        readonly property bool isActiveSort: (panelName === "local" ? root.localSortColumn : root.remoteSortColumn) === colId
        readonly property bool sortAsc: panelName === "local" ? root.localSortAsc : root.remoteSortAsc

        property bool dragActive: false
        property real dragX: 0
        property real pressOffset: 0

        width: root.columnWidthAt(panelName, colId, parentWidth)
        height: parent ? parent.height : 22
        x: dragActive ? dragX : root.slotX(panelName, currentVisualIndex, parentWidth)
        z: isDragged ? 10 : 0

        Behavior on x {
            enabled: !headerCell.dragActive
            NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
        }

        Rectangle {
            anchors.fill: parent
            color: headerCell.dragActive ? "#1e293b" : "transparent"
            radius: 3
            opacity: headerCell.dragActive ? 0.9 : 1
        }

        Label {
            anchors.fill: parent
            anchors.leftMargin: 2
            anchors.rightMargin: 2
            text: root.columnTitle(headerCell.colId)
                  + root.sortIcon(headerCell.colId,
                                  headerCell.panelName === "local" ? root.localSortColumn : root.remoteSortColumn,
                                  headerCell.sortAsc)
            color: headerCell.isActiveSort ? "#60a5fa" : "#93c5fd"
            font.pixelSize: 11
            horizontalAlignment: root.columnAlignsRight(headerCell.colId) ? Text.AlignRight : Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            preventStealing: true

            property bool armed: false
            property real pressMouseX: 0

            onPressed: function(mouse) {
                armed = true
                pressMouseX = mouse.x
                headerCell.pressOffset = mouse.x
            }
            onPositionChanged: function(mouse) {
                if (!armed) return
                if (!headerCell.dragActive && Math.abs(mouse.x - pressMouseX) > 4) {
                    headerCell.dragActive = true
                    root.dragPanel = headerCell.panelName
                    root.dragSourceIndex = headerCell.naturalIndex
                    root.dragTargetIndex = headerCell.naturalIndex
                }
                if (headerCell.dragActive) {
                    const absoluteX = headerCell.x + mouse.x
                    headerCell.dragX = absoluteX - headerCell.pressOffset
                    const center = headerCell.dragX + headerCell.width / 2
                    const target = root.computeTargetIndex(headerCell.panelName, center, headerCell.parentWidth)
                    if (target !== root.dragTargetIndex) {
                        root.dragTargetIndex = target
                    }
                }
            }
            onReleased: function(mouse) {
                const wasDrag = headerCell.dragActive
                const wasArmed = armed
                headerCell.dragActive = false
                armed = false
                if (wasDrag) {
                    Qt.callLater(root.commitColumnDrag)
                } else if (wasArmed) {
                    if (headerCell.panelName === "local") root.sortLocal(headerCell.colId)
                    else root.sortRemote(headerCell.colId)
                }
            }
            onCanceled: {
                const wasDrag = headerCell.dragActive
                headerCell.dragActive = false
                armed = false
                if (wasDrag) Qt.callLater(root.cancelColumnDrag)
            }
        }
    }

    component DataCell: Loader {
        id: dataCell

        property string panelName: "local"
        property string colId: ""
        property int naturalIndex: 0
        property real parentWidth: 0
        property var entry: ({})

        width: root.columnWidthAt(panelName, colId, parentWidth)
        height: parent ? parent.height : 26
        x: root.slotX(panelName, root.visualIndexOf(panelName, naturalIndex), parentWidth)

        Behavior on x {
            enabled: root.dragPanel === dataCell.panelName
            NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
        }

        sourceComponent: {
            switch (colId) {
            case "name": return cellNameComponent
            case "size": return cellSizeComponent
            case "owner": return cellOwnerComponent
            case "permissions": return cellPermComponent
            case "modified": return cellModifiedComponent
            }
            return null
        }
    }

    Component {
        id: cellNameComponent
        Row {
            property var entry: parent && parent.entry ? parent.entry : ({})
            anchors.fill: parent
            spacing: 8

            FileTypeIcon {
                isDir: parent.entry.isDir || false
                accentColor: root.entryNameColor(parent.entry)
                anchors.verticalCenter: parent.verticalCenter
            }
            Label {
                text: parent.entry.name || ""
                color: root.entryNameColor(parent.entry)
                font.pixelSize: 11
                elide: Text.ElideRight
                width: parent.width - 16 - parent.spacing
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    Component {
        id: cellSizeComponent
        Label {
            anchors.fill: parent
            text: root.formatFileSize(parent && parent.entry ? parent.entry.size : "")
            color: "#94a3b8"
            font.pixelSize: 11
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
        }
    }

    Component {
        id: cellOwnerComponent
        Label {
            anchors.fill: parent
            text: parent && parent.entry ? (parent.entry.owner || "") : ""
            color: "#94a3b8"
            font.pixelSize: 11
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    Component {
        id: cellPermComponent
        Label {
            anchors.fill: parent
            text: parent && parent.entry
                  ? root.permissionsToSymbolic(parent.entry.permissions, parent.entry.isDir)
                  : ""
            color: "#94a3b8"
            font.family: "Courier New"
            font.pixelSize: 11
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
        }
    }

    Component {
        id: cellModifiedComponent
        Label {
            anchors.fill: parent
            text: parent && parent.entry ? (parent.entry.modified || "") : ""
            color: "#94a3b8"
            font.pixelSize: 11
            verticalAlignment: Text.AlignVCenter
        }
    }

    Component.onCompleted: {
        refreshLocal()
        switchRemoteSession()
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
            const prefix = root.pendingChmodIsDir ? "d" : "-"
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
                    text: root.pendingChmodName
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
                visible: root.pendingChmodIsDir
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
            root.startRemoteOperation(appController.requestRemoteChmod(root.connectionId,
                                                                        root.pendingChmodPath,
                                                                        octalText()))
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
                root.startRemoteOperation(appController.requestRenameRemotePath(root.connectionId,
                                                                                root.pendingRemotePath,
                                                                                nameField.text))
            } else {
                root.startRemoteOperation(appController.requestCreateRemotePath(root.connectionId,
                                                                                root.remotePath,
                                                                                nameField.text,
                                                                                root.nameDialogMode === "newDir"))
            }
        }
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
            root.saveCurrentRemoteState()
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
            // 目录始终置顶
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
        return asc ? " ▲" : " ▼"
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
        startRemoteOperation(appController.requestRemoteDownload(connectionId, path, folder))
    }

    function openRemotePath(path) {
        startRemoteOperation(appController.requestOpenRemotePath(connectionId, path))
    }

    function openNameDialog(mode, path, currentName) {
        nameDialogMode = mode
        pendingRemotePath = path || ""
        nameField.text = currentName || ""
        nameField.selectAll()
        nameDialog.open()
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
                    message: next[i].message || ""
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
                message: ""
            })
        }
        transferTasks = next.slice(0, 20)
    }

    function finishTransferTask(requestId, ok, message) {
        const next = transferTasks.slice()
        for (let i = 0; i < next.length; ++i) {
            if (next[i].requestId === requestId) {
                const done = ok && next[i].total > 0 ? next[i].total : next[i].done
                next[i] = Object.assign({}, next[i], {
                    done: done,
                    speed: ok ? 0 : next[i].speed,
                    status: ok ? "done" : "failed",
                    message: message || ""
                })
                transferTasks = next
                return
            }
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
            }
        }

        function onRemoteOperationFinished(requestId, connectionId, operation, path, ok, message) {
            root.finishTransferTask(requestId, ok, message)
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
                root.requestRemoteForSession(key, connectionId, base.remotePath || appController.remoteHomePath(connectionId))
            } else if (key === root.sessionKey) {
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

                    Item {
                        id: localHeaderInner
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8

                        Repeater {
                            model: root.localColumnOrder
                            delegate: HeaderCell {
                                required property string modelData
                                required property int index
                                panelName: "local"
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

                    delegate: Rectangle {
                        id: localRow
                        required property var modelData

                        width: localList.width
                        height: 26
                        color: mouseArea.containsMouse ? "#111827" : "#020617"

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
                                delegate: DataCell {
                                    required property string modelData
                                    required property int index
                                    panelName: "local"
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
                            delegate: HeaderCell {
                                required property string modelData
                                required property int index
                                panelName: "remote"
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

                        delegate: Rectangle {
                            id: remoteRow
                            required property var modelData

                            width: remoteList.width
                            height: 26
                            color: remoteRowDropArea.containsDrag ? "#1e3a5f"
                                  : remoteMouseArea.containsMouse ? "#111827" : "#020617"

                            Item {
                                id: remoteRowInner
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8

                                Repeater {
                                    model: root.remoteColumnOrder
                                    delegate: DataCell {
                                        required property string modelData
                                        required property int index
                                        panelName: "remote"
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
                                    if (mouse.button === Qt.RightButton) {
                                        remoteItemMenu.popup()
                                    }
                                }
                                onDoubleClicked: {
                                    if (remoteRow.modelData.isDir) {
                                        root.enterRemote(remoteRow.modelData.path)
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
                                        enabled: !remoteRow.modelData.isDir
                                        onTriggered: root.openRemotePath(remoteRow.modelData.path)
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
                                        onTriggered: appController.copyTextToClipboard(remoteRow.modelData.path)
                                    }
                                    MenuSeparator {}
                                    MenuItem {
                                        text: qsTr("Download")
                                        onTriggered: root.downloadRemotePath(remoteRow.modelData.path)
                                    }
                                    MenuItem {
                                        text: qsTr("Upload...")
                                        enabled: remoteRow.modelData.isDir
                                        onTriggered: {
                                            root.remotePath = remoteRow.modelData.path
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
                                        onTriggered: root.openNameDialog("rename", remoteRow.modelData.path, remoteRow.modelData.name)
                                    }
                                    MenuItem {
                                        text: qsTr("Delete")
                                        onTriggered: root.deleteRemotePath(remoteRow.modelData.path, false)
                                    }
                                    MenuItem {
                                        text: qsTr("Quick Delete (rm)")
                                        onTriggered: root.deleteRemotePath(remoteRow.modelData.path, true)
                                    }
                                    MenuSeparator {}
                                    MenuItem {
                                        text: qsTr("Permissions (%1)").arg(remoteRow.modelData.permissions || qsTr("?"))
                                        onTriggered: root.openChmodDialog(remoteRow.modelData.path,
                                                                          remoteRow.modelData.permissions || "",
                                                                          remoteRow.modelData.isDir)
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

            Rectangle {
                id: transferPanel
                visible: root.transferPanelOpen && root.transferTasks.length > 0
                z: 30
                width: Math.min(420, parent.width - 16)
                height: Math.min(260, parent.height - 48)
                anchors.top: parent.top
                anchors.topMargin: 38
                anchors.right: parent.right
                anchors.rightMargin: 8
                radius: 4
                color: "#020617"
                border.color: "#334155"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            text: qsTr("Transfer tasks")
                            color: "#dbeafe"
                            font.bold: true
                            font.pixelSize: 12
                            Layout.fillWidth: true
                        }
                        ToolButton {
                            implicitWidth: 24
                            implicitHeight: 22
                            contentItem: BroomIcon {
                                anchors.centerIn: parent
                            }
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Clear finished tasks")
                            onClicked: root.clearFinishedTransfers()
                        }
                        ToolButton {
                            implicitWidth: 24
                            implicitHeight: 22
                            text: "×"
                            onClicked: root.transferPanelOpen = false
                        }
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: root.transferTasks
                        spacing: 8

                        delegate: Rectangle {
                            required property var modelData
                            width: ListView.view.width
                            height: 58
                            color: "#0f172a"
                            radius: 4
                            border.color: modelData.status === "failed" ? "#7f1d1d" : "#1e293b"

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 4

                                RowLayout {
                                    Layout.fillWidth: true
                                    Label {
                                        text: modelData.operation === "upload" ? qsTr("Upload") : qsTr("Download")
                                        color: modelData.operation === "upload" ? "#93c5fd" : "#86efac"
                                        font.pixelSize: 11
                                        font.bold: true
                                    }
                                    Label {
                                        text: root.shortPath(modelData.path)
                                        color: "#dbeafe"
                                        font.pixelSize: 11
                                        elide: Text.ElideMiddle
                                        Layout.fillWidth: true
                                    }
                                    Label {
                                        text: modelData.status === "failed"
                                              ? qsTr("Failed")
                                              : modelData.status === "done"
                                                ? qsTr("Done")
                                                : qsTr("%1%").arg(root.transferPercent(modelData))
                                        color: modelData.status === "failed" ? "#fca5a5" : "#94a3b8"
                                        font.pixelSize: 11
                                    }
                                }

                                ProgressBar {
                                    Layout.fillWidth: true
                                    from: 0
                                    to: modelData.total > 0 ? modelData.total : 1
                                    value: modelData.total > 0 ? modelData.done : 0
                                    indeterminate: modelData.status === "running" && modelData.total <= 0
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Label {
                                        text: root.formatFileSize(modelData.done) + " / "
                                              + (modelData.total > 0 ? root.formatFileSize(modelData.total) : qsTr("Unknown"))
                                        color: "#94a3b8"
                                        font.pixelSize: 10
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        text: modelData.status === "running" ? root.formatSpeed(modelData.speed) : modelData.message
                                        color: "#94a3b8"
                                        font.pixelSize: 10
                                        elide: Text.ElideMiddle
                                        Layout.maximumWidth: 140
                                    }
                                }
                            }
                        }

                        ScrollBar.vertical: ScrollBar {}
                    }
                }
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
