import QtQuick
import QtQuick.Controls

TextArea {
    id: area

    property bool classic: false

    clip: true
    wrapMode: TextArea.Wrap
    color: classic ? "#0f172a" : "#e2e8f0"
    selectedTextColor: "#ffffff"
    selectionColor: "#2563eb"

    background: Rectangle {
        radius: 3
        color: area.classic ? "#f8fafc" : "#0f172a"
        border.width: 1
        border.color: area.activeFocus ? "#38bdf8" : (area.classic ? "#cbd5e1" : "#334155")
    }
}
