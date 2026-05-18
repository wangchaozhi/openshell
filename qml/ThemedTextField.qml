import QtQuick
import QtQuick.Controls

TextField {
    id: field

    property bool classic: false
    property bool error: false

    color: classic ? "#0f172a" : "#e2e8f0"
    placeholderTextColor: error ? (classic ? "#dc2626" : "#fca5a5")
                                : (classic ? "#94a3b8" : "#64748b")
    selectedTextColor: "#ffffff"
    selectionColor: "#2563eb"

    background: Rectangle {
        radius: 3
        color: field.classic ? "#f8fafc" : "#0f172a"
        border.width: 1
        border.color: field.error ? "#ef4444"
                     : (field.activeFocus ? "#38bdf8" : (field.classic ? "#cbd5e1" : "#334155"))
    }
}
