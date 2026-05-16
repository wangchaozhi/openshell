import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var session: ({})

    color: "#0f172a"
    border.color: "#1e293b"

    RowLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 4

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#020617"
            radius: 4

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                Label {
                    text: qsTr("Local")
                    color: "#cbd5f5"
                    font.bold: true
                    font.pixelSize: 12
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Local file browser placeholder. Will list the user's home folder once wired.")
                    color: "#64748b"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#020617"
            radius: 4

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                Label {
                    text: qsTr("Remote")
                    color: "#cbd5f5"
                    font.bold: true
                    font.pixelSize: 12
                }
                Label {
                    Layout.fillWidth: true
                    text: root.session && root.session.title
                          ? qsTr("Remote browser for %1 will show /home/<user> after SFTP backend ships.").arg(root.session.title)
                          : qsTr("Open a session to enable SFTP browsing.")
                    color: "#64748b"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
