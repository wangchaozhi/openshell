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
