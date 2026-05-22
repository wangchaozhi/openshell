import QtQuick
import QtQuick.Controls

MenuSeparator {
    id: separator

    required property var theme

    topPadding: 4
    bottomPadding: 4

    contentItem: Rectangle {
        implicitWidth: 160
        implicitHeight: 1
        color: separator.theme.border
    }
}
