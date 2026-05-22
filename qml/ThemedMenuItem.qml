import QtQuick
import QtQuick.Controls

MenuItem {
    id: item

    required property var theme

    implicitWidth: Math.max(168, contentItem.implicitWidth + leftPadding + rightPadding + 18)
    implicitHeight: 30
    leftPadding: 12
    rightPadding: 24
    topPadding: 0
    bottomPadding: 0

    arrow: Text {
        x: item.width - width - 10
        y: (item.height - height) / 2
        visible: item.subMenu
        text: ">"
        color: item.enabled ? item.theme.textMuted : item.theme.borderMuted
        font.pixelSize: 16
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    indicator: Item {
        implicitWidth: 0
        implicitHeight: 0
    }

    contentItem: Text {
        text: item.text
        color: item.enabled
               ? (item.highlighted ? item.theme.textOnAccent : item.theme.textPrimary)
               : item.theme.textMuted
        opacity: item.enabled ? 1 : 0.55
        font.pixelSize: 12
        elide: Text.ElideMiddle
        horizontalAlignment: Text.AlignLeft
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: 3
        color: item.highlighted && item.enabled ? item.theme.focus : "transparent"
    }
}
