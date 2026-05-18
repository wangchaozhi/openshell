import QtQuick
import QtQuick.Controls

SpinBox {
    id: spin

    property bool classic: false

    leftPadding: 28
    rightPadding: 28
    topPadding: 0
    bottomPadding: 0

    contentItem: TextInput {
        z: 2
        text: spin.textFromValue(spin.value, spin.locale)
        color: spin.classic ? "#0f172a" : "#e2e8f0"
        selectionColor: "#2563eb"
        selectedTextColor: "#ffffff"
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        readOnly: !spin.editable
        validator: spin.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        onEditingFinished: spin.value = spin.valueFromText(text, spin.locale)
    }

    up.indicator: Rectangle {
        x: spin.mirrored ? 0 : parent.width - width
        height: parent.height
        width: 24
        radius: 0
        color: spin.up.pressed
               ? (spin.classic ? "#bae6fd" : "#1e40af")
               : spin.up.hovered ? (spin.classic ? "#e0f2fe" : "#1e3a8a") : "transparent"
        Text {
            text: "+"
            anchors.centerIn: parent
            color: spin.classic ? "#334155" : "#94a3b8"
            font.pixelSize: 14
        }
        HoverHandler {
            cursorShape: Qt.PointingHandCursor
        }
    }

    down.indicator: Rectangle {
        x: spin.mirrored ? parent.width - width : 0
        height: parent.height
        width: 24
        radius: 0
        color: spin.down.pressed
               ? (spin.classic ? "#bae6fd" : "#1e40af")
               : spin.down.hovered ? (spin.classic ? "#e0f2fe" : "#1e3a8a") : "transparent"
        Text {
            text: "−"
            anchors.centerIn: parent
            color: spin.classic ? "#334155" : "#94a3b8"
            font.pixelSize: 14
        }
        HoverHandler {
            cursorShape: Qt.PointingHandCursor
        }
    }

    background: Rectangle {
        radius: 3
        color: spin.classic ? "#f8fafc" : "#0f172a"
        border.width: 1
        border.color: spin.activeFocus ? "#38bdf8" : (spin.classic ? "#cbd5e1" : "#334155")
    }
}
