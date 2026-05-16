import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    property color accentColor: "#0ea5e9"
    property real contentPreferredHeight: -1
    property alias contentSpacing: contentLayout.spacing
    default property alias content: contentLayout.data

    Layout.fillWidth: true
    Layout.preferredHeight: contentPreferredHeight > 0 ? contentPreferredHeight : contentLayout.implicitHeight + 34
    radius: 10
    color: "#0b1220"
    border.color: "#1e293b"

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 4
        radius: 10
        color: root.accentColor
    }

    ColumnLayout {
        id: contentLayout
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        anchors.topMargin: 16
        anchors.bottomMargin: 18
        spacing: 12
    }
}
