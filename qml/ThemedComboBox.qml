import QtQuick
import QtQuick.Controls

ComboBox {
    id: combo

    property bool classic: false

    contentItem: Label {
        leftPadding: 8
        rightPadding: 22
        verticalAlignment: Text.AlignVCenter
        text: combo.displayText
        color: combo.classic ? "#0f172a" : "#e2e8f0"
        elide: Text.ElideRight
        font.pixelSize: 13
    }

    indicator: Canvas {
        x: combo.width - width - 8
        y: (combo.height - height) / 2
        width: 8
        height: 5
        contextType: "2d"
        onPaint: {
            context.reset()
            context.moveTo(0, 0)
            context.lineTo(width, 0)
            context.lineTo(width / 2, height)
            context.closePath()
            context.fillStyle = combo.classic ? "#475569" : "#94a3b8"
            context.fill()
        }
    }

    background: Rectangle {
        radius: 3
        color: combo.classic ? "#f8fafc" : "#0f172a"
        border.width: 1
        border.color: combo.activeFocus || combo.down ? "#38bdf8" : (combo.classic ? "#cbd5e1" : "#334155")
    }

    delegate: ItemDelegate {
        width: combo.width
        height: 28
        highlighted: combo.highlightedIndex === index
        contentItem: Label {
            text: modelData
            color: highlighted ? "#ffffff" : (combo.classic ? "#0f172a" : "#e2e8f0")
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 13
        }
        background: Rectangle {
            color: highlighted ? "#2563eb" : (combo.classic ? "#ffffff" : "#0f172a")
        }
    }

    popup: Popup {
        y: combo.height + 2
        width: combo.width
        implicitHeight: Math.min(contentItem.implicitHeight + 2, 160)
        padding: 1
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: combo.popup.visible ? combo.delegateModel : null
            currentIndex: combo.highlightedIndex
        }
        background: Rectangle {
            color: combo.classic ? "#ffffff" : "#0f172a"
            border.color: combo.classic ? "#cbd5e1" : "#334155"
            radius: 3
        }
    }

    HoverHandler {
        cursorShape: Qt.PointingHandCursor
    }
}
