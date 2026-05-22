import QtQuick

Item {
    id: root

    property bool active: true
    property bool connected: true
    property bool problem: false
    property color maskColor: "#020617"
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
        border.color: root.strokeColor
        border.width: 2
    }
    Rectangle {
        x: 7
        y: 1
        width: 5
        height: 5
        rotation: 45
        color: root.accentColor
    }
    Rectangle {
        x: 3
        y: 9
        width: 5
        height: 5
        rotation: 45
        color: root.accentColor
    }
    Rectangle {
        x: 2
        y: 5
        width: 5
        height: 5
        color: root.maskColor
    }
    Rectangle {
        x: 8
        y: 5
        width: 5
        height: 5
        color: root.maskColor
    }
    Rectangle {
        x: 6
        y: 6
        width: 3
        height: 3
        radius: 1
        color: root.dotColor
    }
}
