import QtQuick

Item {
    id: root

    property color color: "#93c5fd"
    property color maskColor: "#020617"

    implicitWidth: 15
    implicitHeight: 15
    Rectangle {
        x: 2
        y: 2
        width: 11
        height: 11
        radius: 6
        color: "transparent"
        border.color: root.color
        border.width: 2
    }
    Rectangle {
        x: 9
        y: 1
        width: 5
        height: 5
        rotation: 45
        color: root.color
    }
    Rectangle {
        x: 0
        y: 9
        width: 5
        height: 5
        rotation: 45
        color: root.color
    }
    Rectangle {
        x: 1
        y: 6
        width: 5
        height: 4
        color: root.maskColor
    }
    Rectangle {
        x: 9
        y: 5
        width: 5
        height: 4
        color: root.maskColor
    }
}
