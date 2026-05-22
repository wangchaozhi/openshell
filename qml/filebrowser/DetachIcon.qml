import QtQuick

Item {
    id: root

    property color color: "#93c5fd"
    property color accentColor: "#60a5fa"

    implicitWidth: 15
    implicitHeight: 15

    Rectangle {
        x: 1
        y: 1
        width: 10
        height: 10
        radius: 2
        color: "transparent"
        border.color: root.color
        border.width: 1
    }

    Rectangle {
        x: 5
        y: 5
        width: 9
        height: 9
        radius: 2
        color: "transparent"
        border.color: root.accentColor
        border.width: 2
    }

    Rectangle {
        x: 9
        y: 2
        width: 5
        height: 2
        radius: 1
        color: root.color
    }

    Rectangle {
        x: 12
        y: 2
        width: 2
        height: 5
        radius: 1
        color: root.color
    }
}
