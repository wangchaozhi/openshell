import QtQuick
import QtQuick.Controls

Dialog {
    id: root

    property var pending: []
    property var request: ({})
    title: request.previous ? qsTr("SSH host key changed") : qsTr("Verify SSH host key")
    modal: true
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 32 : 400, 480)
    standardButtons: Dialog.Ok | Dialog.Cancel
    closePolicy: Popup.CloseOnEscape

    contentItem: Label {
        wrapMode: Text.Wrap
        text: (root.request.previous
               ? qsTr("The SSH key for %1 changed. Verify the new fingerprint independently before accepting it.\n\nPrevious: %2\nCurrent: %3")
                   .arg(root.request.endpoint).arg(root.request.previous).arg(root.request.fingerprint)
               : qsTr("Verify this fingerprint for %1 before trusting the server.\n\n%2")
                   .arg(root.request.endpoint || "").arg(root.request.fingerprint || ""))
    }

    function showNext() {
        if (pending.length > 0) {
            request = pending[0]
            pending = pending.slice(1)
            open()
        }
    }

    function respond(trusted) {
        if (request.id) {
            appController.resolveSshHostKey(request.id, trusted)
            request = ({})
        }
    }

    onAccepted: respond(true)
    onRejected: respond(false)
    onClosed: {
        respond(false)
        Qt.callLater(showNext)
    }

    Connections {
        target: appController
        function onSshHostKeyConfirmationRequested(requestId, endpoint, fingerprint, previousFingerprint) {
            root.pending = root.pending.concat([{
                id: requestId, endpoint: endpoint, fingerprint: fingerprint, previous: previousFingerprint
            }])
            if (!root.visible && !root.request.id) {
                root.showNext()
            }
        }
    }
}
