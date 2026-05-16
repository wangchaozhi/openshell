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
                            hoverEnabled: true
                            onDoubleClicked: {
                                if (modelData.isDir) {
                                    root.enterLocal(modelData.path)
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
                                    text: modelData.modified
                                    color: "#94a3b8"
                                    font.pixelSize: 11
                                    Layout.preferredWidth: 118
                                }
                            }

                            MouseArea {
                                id: remoteMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                onDoubleClicked: {
                                    if (modelData.isDir) {
                                        root.enterRemote(modelData.path)
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
