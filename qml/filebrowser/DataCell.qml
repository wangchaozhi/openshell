import QtQuick
import QtQuick.Controls

Loader {
    id: root

    required property var fileBrowser
    property string panelName: "local"
    property string colId: ""
    property int naturalIndex: 0
    property real parentWidth: 0
    property var entry: ({})

    component FileTypeIcon: Item {
        property bool isDir: false
        property color accentColor: isDir ? "#3b82f6" : "#94a3b8"

        implicitWidth: 16
        implicitHeight: 16

        Rectangle {
            x: 2
            y: 2
            width: 12
            height: 12
            radius: 2
            color: parent.isDir ? "#1d4ed8" : "#334155"
            border.color: parent.accentColor
        }

        Rectangle {
            visible: parent.isDir
            x: 2
            y: 1
            width: 7
            height: 4
            radius: 1
            color: "#60a5fa"
        }

        Rectangle {
            visible: !parent.isDir
            x: 9
            y: 2
            width: 5
            height: 5
            color: parent.accentColor
            opacity: 0.35
        }

        Rectangle {
            visible: !parent.isDir
            x: 5
            y: 7
            width: 6
            height: 1
            color: parent.accentColor
        }
    }

    width: fileBrowser.columnWidthAt(panelName, colId, parentWidth)
    height: parent ? parent.height : 26
    x: fileBrowser.slotX(panelName, fileBrowser.visualIndexOf(panelName, naturalIndex), parentWidth)

    Behavior on x {
        enabled: root.fileBrowser.dragPanel === root.panelName
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

    Component {
        id: cellNameComponent

        Row {
            property var entry: root.entry || ({})

            anchors.fill: parent
            spacing: 8

            FileTypeIcon {
                isDir: parent.entry.isDir || false
                accentColor: root.fileBrowser.entryNameColor(parent.entry)
                anchors.verticalCenter: parent.verticalCenter
            }

            Label {
                text: parent.entry.name || ""
                color: root.fileBrowser.entryNameColor(parent.entry)
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
            text: root.fileBrowser.formatFileSize(root.entry ? root.entry.size : "")
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
            text: root.entry ? (root.entry.owner || "") : ""
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
            text: root.entry
                  ? root.fileBrowser.permissionsToSymbolic(root.entry.permissions, root.entry.isDir)
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
            text: root.entry ? (root.entry.modified || "") : ""
            color: "#94a3b8"
            font.pixelSize: 11
            verticalAlignment: Text.AlignVCenter
        }
    }
}
