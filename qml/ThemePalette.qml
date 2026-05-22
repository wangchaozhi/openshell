import QtQuick

QtObject {
    id: root

    property string mode: "dark"

    readonly property bool classic: mode === "classic"
    readonly property bool forest: mode === "forest"

    readonly property color window: classic ? "#f8fafc" : forest ? "#07130f" : "#0f172a"
    readonly property color panel: classic ? "#ffffff" : forest ? "#0b1b15" : "#020617"
    readonly property color surface: classic ? "#f8fafc" : forest ? "#10261d" : "#0f172a"
    readonly property color surfaceRaised: classic ? "#ffffff" : forest ? "#163629" : "#111827"
    readonly property color hover: classic ? "#f1f5f9" : forest ? "#17382b" : "#111827"
    readonly property color selected: classic ? "#e0f2fe" : forest ? "#14532d" : "#172554"
    readonly property color dropHover: classic ? "#dbeafe" : forest ? "#166534" : "#1e3a5f"

    readonly property color border: classic ? "#cbd5e1" : forest ? "#1f4d3a" : "#1e293b"
    readonly property color borderMuted: classic ? "#e2e8f0" : forest ? "#2d6a4f" : "#334155"
    readonly property color focus: forest ? "#34d399" : "#38bdf8"

    readonly property color textPrimary: classic ? "#0f172a" : forest ? "#ecfdf5" : "#dbeafe"
    readonly property color textSecondary: classic ? "#334155" : forest ? "#bbf7d0" : "#cbd5f5"
    readonly property color textMuted: classic ? "#64748b" : forest ? "#86efac" : "#94a3b8"
    readonly property color textHeader: classic ? "#0369a1" : forest ? "#6ee7b7" : "#93c5fd"
    readonly property color textActive: classic ? "#0284c7" : forest ? "#34d399" : "#60a5fa"
    readonly property color textOnAccent: "#ffffff"

    readonly property color icon: classic ? "#475569" : forest ? "#86efac" : "#93c5fd"
    readonly property color iconAccent: classic ? "#0284c7" : forest ? "#34d399" : "#60a5fa"

    readonly property color success: classic ? "#16a34a" : forest ? "#4ade80" : "#34d399"
    readonly property color warning: classic ? "#d97706" : forest ? "#facc15" : "#fbbf24"
    readonly property color danger: classic ? "#dc2626" : forest ? "#fb7185" : "#f87171"
    readonly property color dangerSoft: classic ? "#fee2e2" : forest ? "#4c101f" : "#450a0a"
}
