import QtQuick

Item {
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
