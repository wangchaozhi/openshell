import QtQuick
import QtQuick.Controls

ComboBox {
    textRole: "label"
    valueRole: "value"
    model: [
        { label: qsTr("Rename new files"), value: "rename" },
        { label: qsTr("Skip existing files"), value: "skip" },
        { label: qsTr("Overwrite existing files"), value: "overwrite" }
    ]
    Accessible.name: qsTr("Transfer conflicts")
    Component.onCompleted: currentIndex = Math.max(0, indexOfValue(appController.transferConflictPolicy()))
    onActivated: appController.setTransferConflictPolicy(currentValue)
}
