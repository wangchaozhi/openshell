import QtQuick
import QtQuick.Controls

Button {
    id: btn

    property bool classic: false
    property bool primary: false

    implicitWidth: 88
    implicitHeight: 32
    hoverEnabled: true

    background: Rectangle {
        radius: 4
        color: btn.primary
               ? (btn.hovered ? "#1d4ed8" : "#2563eb")
               : (btn.hovered
                  ? (btn.classic ? "#e2e8f0" : "#1e3a5f")
                  : (btn.classic ? "#f1f5f9" : "#334155"))
        border.width: btn.primary ? 0 : 1
        border.color: btn.classic ? "#cbd5e1" : "#475569"
    }

    contentItem: Label {
        text: btn.text
        color: btn.primary ? "#ffffff" : (btn.classic ? "#334155" : "#e2e8f0")
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: 13
    }

    HoverHandler {
        cursorShape: Qt.PointingHandCursor
    }
}
