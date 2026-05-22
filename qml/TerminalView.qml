import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var session: ({})
    property string uiTheme: "dark"
    readonly property string sessionId: session && session.id ? session.id : ""
    readonly property bool sessionConnected: session && session.status === "connected"
    property string clipboardTextSnapshot: ""
    property string detectedRemotePath: ""
    property alias themePalette: theme

    signal remoteDirectoryDetected(string path)

    color: theme.panel

    ThemePalette {
        id: theme
        mode: root.uiTheme
    }

    component MenuGlyph: Label {
        property string glyph: ""
        property bool itemEnabled: true

        text: glyph
        color: itemEnabled ? theme.textSecondary : theme.textMuted
        font.family: Qt.platform.os === "osx" ? "Apple Symbols" : "Segoe MDL2 Assets"
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
            color: parent.itemEnabled ? theme.textPrimary : theme.textMuted
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
            color: theme.surface

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12

                Label {
                    text: root.session && root.session.title ? root.session.title : qsTr("(no session)")
                    color: theme.textSecondary
                    font.pixelSize: 12
                    Layout.fillWidth: true
                }

                Label {
                    text: root.session && root.session.status ? root.session.status : ""
                    color: {
                        const s = root.session && root.session.status
                        if (s === "connected") return theme.success
                        if (s === "connecting") return theme.warning
                        if (s === "error") return theme.danger
                        return theme.textMuted
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
                fontFamily: Qt.platform.os === "osx" ? "Menlo" : "Consolas"
                fontPixelSize: 14
                background: theme.panel
                foreground: theme.textPrimary
                cursorColor: theme.focus
                cursorBlinking: root.sessionConnected
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

                ThemedMenu {
                    id: terminalMenu
                    menuTheme: root.themePalette
                    onAboutToShow: root.refreshTerminalMenu()

                    ThemedMenuItem {
                        id: copyMenuItem
                        theme: root.themePalette
                        text: qsTr("Copy")
                        enabled: terminal.hasSelection
                        contentItem: TerminalMenuContent {
                            glyph: ""
                            label: copyMenuItem.text
                            itemEnabled: copyMenuItem.enabled
                        }
                        onTriggered: root.copySelection()
                    }
                    ThemedMenuItem {
                        id: pasteMenuItem
                        theme: root.themePalette
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
                    ThemedMenuItem {
                        id: selectAllMenuItem
                        theme: root.themePalette
                        text: qsTr("Select All")
                        enabled: root.sessionId !== "" && terminal.screen !== null
                        contentItem: TerminalMenuContent {
                            glyph: ""
                            label: selectAllMenuItem.text
                            itemEnabled: selectAllMenuItem.enabled
                        }
                        onTriggered: terminal.selectAll()
                    }
                    ThemedMenuSeparator { theme: root.themePalette }
                    ThemedMenuItem {
                        id: sendCtrlCMenuItem
                        theme: root.themePalette
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
                    ThemedMenuItem {
                        id: clearScreenMenuItem
                        theme: root.themePalette
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
