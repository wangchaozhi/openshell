import QtQuick
import QtQuick.Controls

Item {
    id: root

    required property var fileBrowser
    property string panelName: "local"
    property string colId: ""
    property int naturalIndex: 0
    property real parentWidth: 0

    readonly property bool isDragged: fileBrowser.dragPanel === panelName
                                      && fileBrowser.dragSourceIndex === naturalIndex
    readonly property int currentVisualIndex: isDragged ? fileBrowser.dragTargetIndex
                                                        : fileBrowser.visualIndexOf(panelName, naturalIndex)
    readonly property bool isActiveSort: (panelName === "local"
                                          ? fileBrowser.localSortColumn
                                          : fileBrowser.remoteSortColumn) === colId
    readonly property bool sortAsc: panelName === "local"
                                    ? fileBrowser.localSortAsc
                                    : fileBrowser.remoteSortAsc

    property bool dragActive: false
    property real dragX: 0
    property real pressOffset: 0

    width: fileBrowser.columnWidthAt(panelName, colId, parentWidth)
    height: parent ? parent.height : 22
    x: dragActive ? dragX : fileBrowser.slotX(panelName, currentVisualIndex, parentWidth)
    z: isDragged ? 10 : 0

    Behavior on x {
        enabled: !root.dragActive
        NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
    }

    Rectangle {
        anchors.fill: parent
        color: root.dragActive ? "#1e293b" : "transparent"
        radius: 3
        opacity: root.dragActive ? 0.9 : 1
    }

    Label {
        anchors.fill: parent
        anchors.leftMargin: 2
        anchors.rightMargin: 2
        text: root.fileBrowser.columnTitle(root.colId)
              + root.fileBrowser.sortIcon(root.colId,
                                          root.panelName === "local"
                                          ? root.fileBrowser.localSortColumn
                                          : root.fileBrowser.remoteSortColumn,
                                          root.sortAsc)
        color: root.isActiveSort ? "#60a5fa" : "#93c5fd"
        font.pixelSize: 11
        horizontalAlignment: root.fileBrowser.columnAlignsRight(root.colId)
                             ? Text.AlignRight
                             : Text.AlignLeft
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
            root.pressOffset = mouse.x
        }

        onPositionChanged: function(mouse) {
            if (!armed) return
            if (!root.dragActive && Math.abs(mouse.x - pressMouseX) > 4) {
                root.dragActive = true
                root.fileBrowser.dragPanel = root.panelName
                root.fileBrowser.dragSourceIndex = root.naturalIndex
                root.fileBrowser.dragTargetIndex = root.naturalIndex
            }
            if (root.dragActive) {
                const absoluteX = root.x + mouse.x
                root.dragX = absoluteX - root.pressOffset
                const center = root.dragX + root.width / 2
                const target = root.fileBrowser.computeTargetIndex(root.panelName,
                                                                   center,
                                                                   root.parentWidth)
                if (target !== root.fileBrowser.dragTargetIndex) {
                    root.fileBrowser.dragTargetIndex = target
                }
            }
        }

        onReleased: {
            const wasDrag = root.dragActive
            const wasArmed = armed
            root.dragActive = false
            armed = false
            if (wasDrag) {
                Qt.callLater(root.fileBrowser.commitColumnDrag)
            } else if (wasArmed) {
                if (root.panelName === "local") root.fileBrowser.sortLocal(root.colId)
                else root.fileBrowser.sortRemote(root.colId)
            }
        }

        onCanceled: {
            const wasDrag = root.dragActive
            root.dragActive = false
            armed = false
            if (wasDrag) Qt.callLater(root.fileBrowser.cancelColumnDrag)
        }
    }
}
