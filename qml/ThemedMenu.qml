import QtQuick
import QtQuick.Controls

Menu {
    id: menu

    required property var menuTheme

    modal: false
    dim: false
    padding: 4

    delegate: ThemedMenuItem {
        theme: menuTheme
    }

    background: Rectangle {
        implicitWidth: 180
        color: menuTheme.surfaceRaised
        border.color: menuTheme.border
        radius: 4
    }
}
