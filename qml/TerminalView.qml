import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import OpenShell 1.0

Rectangle {
    id: root

    property var session: ({})
    readonly property string sessionId: session && session.id ? session.id : ""
    property string clipboardTextSnapshot: ""
    property string detectedRemotePath: ""

    signal remoteDirectoryDetected(string path)

    color: "#020617"

    component MenuGlyph: Label {
        property string glyph: ""
        property bool itemEnabled: true

        text: glyph
        color: itemEnabled ? "#334155" : "#cbd5e1"
        font.family: "Segoe MDL2 Assets"
        font.pixelSize: 13
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    component TerminalMenuContent: RowLayout {
        property string glyph: ""
        property string label: ""
        property bool itemEnabled: true

        spacing: 8
        MenuGlyph {
            glyph: parent.glyph
            itemEnabled: parent.itemEnabled
            Layout.preferredWidth: 18
        }
        Label {
            text: parent.label
            color: parent.itemEnabled ? "#111827" : "#cbd5e1"
            Layout.fillWidth: true
        }
    }

    function refreshTerminalMenu() {
        clipboardTextSnapshot = appController.clipboardText()
    }

    function copySelection() {
        if (terminal.hasSelection && terminal.selectedText.length > 0) {
            appController.copyTextToClipboard(terminal.selectedText)
        }
    }

    function bindScreen() {
        if (sessionId === "") {
            terminal.screen = null
            detectedRemotePath = ""
            return
        }
        const next = appController.sessionScreen(sessionId)
        if (next === terminal.screen) {
            return
        }
        terminal.screen = next
        terminal.requestFocus()
        promptPathProbe.restart()
    }

    function normalizePromptPath(path) {
        if (!path || path.length === 0) {
            return ""
        }
        let clean = path.trim()
        clean = clean.replace(/\\\s/g, " ")
        if (clean === "~" || clean.indexOf("~/") === 0 || clean.charAt(0) === "/") {
            return clean
        }
        return ""
    }

    function detectPromptPathFromSnapshot(snapshot) {
        if (!snapshot || snapshot.length === 0) {
            return ""
        }
        const lines = snapshot.split(/\r?\n/)
        for (let i = lines.length - 1; i >= 0; --i) {
            const line = lines[i].trim()
            if (line.length === 0) {
                continue
            }
            let match = line.match(/(?:^|\s)[^@\s]+@[^:\s]+:(~(?:\/[^\s#\$]*)?|\/[^\s#\$]*)[#$]\s*$/)
            if (match && match[1]) {
                return normalizePromptPath(match[1])
            }
            match = line.match(/(?:^|\s)(~(?:\/[^\s#\$]*)?|\/[^\s#\$]*)[#$]\s*$/)
            if (match && match[1]) {
                return normalizePromptPath(match[1])
            }
        }
        return ""
    }

    function probePromptPath() {
        if (!terminal.screen || !terminal.screen.plainTextSnapshot) {
            return
        }
        const path = detectPromptPathFromSnapshot(terminal.screen.plainTextSnapshot())
        if (path.length > 0 && path !== detectedRemotePath) {
            detectedRemotePath = path
            remoteDirectoryDetected(path)
        }
    }

    onSessionIdChanged: {
        detectedRemotePath = ""
        bindScreen()
    }

    Connections {
        target: appController
        function onSessionsChanged() {
            bindScreen()
        }
    }

    Timer {
        id: promptPathProbe
        interval: 600
        repeat: true
        running: root.sessionId !== "" && terminal.screen !== null
        onTriggered: root.probePromptPath()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 0
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            color: "#0f172a"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12

                Label {
                    text: root.session && root.session.title ? root.session.title : qsTr("(no session)")
                    color: "#cbd5f5"
                    font.pixelSize: 12
                    Layout.fillWidth: true
                }

                Label {
                    text: root.session && root.session.status ? root.session.status : ""
                    color: {
                        const s = root.session && root.session.status
                        if (s === "connected") return "#34d399"
                        if (s === "connecting") return "#fbbf24"
                        if (s === "error") return "#f87171"
                        return "#94a3b8"
                    }
                    font.pixelSize: 11
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            TerminalScreen {
                id: terminal
                anchors.fill: parent
                focus: true
                activeFocusOnTab: true
                fontFamily: "Consolas"
                fontPixelSize: 14
                background: "#020617"
                cursorColor: "#38bdf8"
                onCellSizeRequested: function(cols, rows) {
                    if (root.sessionId !== "") {
                        appController.resizeSession(root.sessionId, cols, rows)
                    }
                }
                onCopySelectionRequested: function(text) {
                    appController.copyTextToClipboard(text)
                }
                Component.onCompleted: bindScreen()

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    propagateComposedEvents: true
                    onClicked: function(mouse) {
                        if (mouse.button === Qt.RightButton) {
                            terminalMenu.popup()
                        } else {
                            mouse.accepted = false
                        }
                    }
                }

                Menu {
                    id: terminalMenu
                    onAboutToShow: root.refreshTerminalMenu()

                    MenuItem {
                        id: copyMenuItem
                        text: qsTr("Copy")
                        enabled: terminal.hasSelection
                        contentItem: TerminalMenuContent {
                            glyph: ""
                            label: copyMenuItem.text
                            itemEnabled: copyMenuItem.enabled
                        }
                        onTriggered: root.copySelection()
                    }
                    MenuItem {
                        id: pasteMenuItem
                        text: qsTr("Paste")
                        enabled: root.sessionId !== "" && root.clipboardTextSnapshot.length > 0
                        contentItem: TerminalMenuContent {
                            glyph: ""
                            label: pasteMenuItem.text
                            itemEnabled: pasteMenuItem.enabled
                        }
                        onTriggered: {
                            if (root.sessionId !== "") {
                                appController.sendSessionInput(root.sessionId,
                                                               root.clipboardTextSnapshot)
                            }
                        }
                    }
                    MenuItem {
                        id: selectAllMenuItem
                        text: qsTr("Select All")
                        enabled: root.sessionId !== "" && terminal.screen !== null
                        contentItem: TerminalMenuContent {
                            glyph: ""
                            label: selectAllMenuItem.text
                            itemEnabled: selectAllMenuItem.enabled
                        }
                        onTriggered: terminal.selectAll()
                    }
                    MenuSeparator {}
                    MenuItem {
                        id: sendCtrlCMenuItem
                        text: qsTr("Send Ctrl+C")
                        enabled: root.sessionId !== ""
                        contentItem: TerminalMenuContent {
                            glyph: ""
                            label: sendCtrlCMenuItem.text
                            itemEnabled: sendCtrlCMenuItem.enabled
                        }
                        onTriggered: {
                            if (root.sessionId !== "") {
                                appController.sendSessionCtrlC(root.sessionId)
                            }
                        }
                    }
                    MenuItem {
                        id: clearScreenMenuItem
                        text: qsTr("Clear Screen")
                        enabled: root.sessionId !== ""
                        contentItem: TerminalMenuContent {
                            glyph: ""
                            label: clearScreenMenuItem.text
                            itemEnabled: clearScreenMenuItem.enabled
                        }
                        onTriggered: {
                            if (root.sessionId !== "") {
                                appController.clearSessionBuffer(root.sessionId)
                            }
                        }
                    }
                }
            }
        }
    }
}
