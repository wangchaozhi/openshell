import QtQuick

Item {
    id: root

    property string direction: "up"
    property color color: "#93c5fd"

    implicitWidth: 15
    implicitHeight: 15

    Canvas {
        id: canvas

        anchors.fill: parent
        antialiasing: true

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = root.color
            ctx.lineWidth = 2.1
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            ctx.beginPath()
            if (root.direction === "left") {
                ctx.moveTo(8, 3.5)
                ctx.lineTo(3, 7.5)
                ctx.lineTo(8, 11.5)
                ctx.moveTo(3.5, 7.5)
                ctx.lineTo(13, 7.5)
            } else if (root.direction === "right") {
                ctx.moveTo(7, 3.5)
                ctx.lineTo(12, 7.5)
                ctx.lineTo(7, 11.5)
                ctx.moveTo(11.5, 7.5)
                ctx.lineTo(2, 7.5)
            } else if (root.direction === "down") {
                ctx.moveTo(3.5, 7)
                ctx.lineTo(7.5, 12)
                ctx.lineTo(11.5, 7)
                ctx.moveTo(7.5, 11.5)
                ctx.lineTo(7.5, 2)
            } else {
                ctx.moveTo(3.5, 7)
                ctx.lineTo(7.5, 2.5)
                ctx.lineTo(11.5, 7)
                ctx.moveTo(7.5, 3)
                ctx.lineTo(7.5, 14)
            }
            ctx.stroke()
        }

        Connections {
            target: root
            function onColorChanged() { canvas.requestPaint() }
            function onDirectionChanged() { canvas.requestPaint() }
        }
    }
}
