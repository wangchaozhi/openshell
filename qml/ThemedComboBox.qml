import QtQuick
import QtQuick.Controls

ComboBox {
    id: combo

    property bool classic: false
    property var menuTheme: null
    readonly property bool hasTheme: menuTheme !== null
    readonly property color themeSurface: hasTheme ? menuTheme.surface : (classic ? "#f8fafc" : "#0f172a")
    readonly property color themeSurfaceRaised: hasTheme ? menuTheme.surfaceRaised : (classic ? "#ffffff" : "#0f172a")
    readonly property color themeTextPrimary: hasTheme ? menuTheme.textPrimary : (classic ? "#0f172a" : "#e2e8f0")
    readonly property color themeTextMuted: hasTheme ? menuTheme.textMuted : (classic ? "#475569" : "#94a3b8")
    readonly property color themeBorderMuted: hasTheme ? menuTheme.borderMuted : (classic ? "#cbd5e1" : "#334155")
    readonly property color themeFocus: hasTheme ? menuTheme.focus : "#38bdf8"
    readonly property color themeTextOnAccent: hasTheme ? menuTheme.textOnAccent : "#ffffff"
    readonly property color themeSelected: hasTheme ? menuTheme.textActive : "#2563eb"

    onThemeTextMutedChanged: indicator.requestPaint()

    contentItem: Label {
        leftPadding: 8
        rightPadding: 22
        verticalAlignment: Text.AlignVCenter
        text: combo.displayText
        color: combo.themeTextPrimary
        elide: Text.ElideRight
        font.pixelSize: 13
    }

    indicator: Canvas {
        id: indicator

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
            context.fillStyle = combo.themeTextMuted
            context.fill()
        }
    }

    background: Rectangle {
        radius: 3
        color: combo.themeSurface
        border.width: 1
        border.color: combo.activeFocus || combo.down ? combo.themeFocus : combo.themeBorderMuted
    }

    delegate: ItemDelegate {
        width: combo.width
        height: 28
        highlighted: combo.highlightedIndex === index
        contentItem: Label {
            text: modelData
            color: highlighted ? combo.themeTextOnAccent : combo.themeTextPrimary
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 13
        }
        background: Rectangle {
            color: highlighted ? combo.themeSelected : combo.themeSurfaceRaised
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
            color: combo.themeSurfaceRaised
            border.color: combo.themeBorderMuted
            radius: 3
        }
    }

    HoverHandler {
        cursorShape: Qt.PointingHandCursor
    }
}
