pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.FluentWinUI3
import Odizinne.QontrolPanel

ColumnLayout {
    id: root
    spacing: 3

    Label {
        text: qsTr("App Volume Hotkeys")
        font.pixelSize: 22
        font.bold: true
        Layout.bottomMargin: 15
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true

        CustomScrollView {
            anchors.fill: parent

            Column {
                width: parent.width
                spacing: 3

                Card {
                    width: parent.width
                    title: qsTr("Per-application volume shortcuts")
                    description: qsTr("Assign global hotkeys to control volume of specific applications")
                    additionalControl: Button {
                        text: qsTr("Add")
                        enabled: UserSettings.globalShortcutsEnabled
                        onClicked: addHotkeyDialog.open()
                    }
                }

                Repeater {
                    id: hotkeyRepeater
                    model: KeyboardShortcutManager.appVolumeHotkeys

                    Card {
                        id: hotkeyCard
                        required property var modelData
                        required property int index
                        width: parent.width
                        title: hotkeyCard.modelData.executableName
                        description: "▲ " + Context.getShortcutText(hotkeyCard.modelData.volumeUpModifiers, hotkeyCard.modelData.volumeUpKey) +
                                     "  ▼ " + Context.getShortcutText(hotkeyCard.modelData.volumeDownModifiers, hotkeyCard.modelData.volumeDownKey) +
                                     (hotkeyCard.modelData.volumeStepSize > 0 ? "  (" + qsTr("Step") + ": " + hotkeyCard.modelData.volumeStepSize + ")" : "")

                        additionalControl: Button {
                            text: qsTr("Remove")
                            onClicked: {
                                KeyboardShortcutManager.removeAppVolumeHotkey(hotkeyCard.modelData.executableName)
                            }
                        }
                    }
                }
            }
        }

        Dialog {
            id: addHotkeyDialog
            title: qsTr("Add App Volume Hotkey")
            modal: true
            width: 450
            anchors.centerIn: parent

            property int volUpKey: Qt.Key_unknown
            property int volUpMods: 0
            property int volDownKey: Qt.Key_unknown
            property int volDownMods: 0
            property string selectedApp: ""
            property bool capturingUp: false
            property bool capturingDown: false
            property int customStepSize: 2

            onOpened: {
                KeyboardShortcutManager.globalShortcutsSuspended = true
                volUpKey = Qt.Key_unknown
                volUpMods = 0
                volDownKey = Qt.Key_unknown
                volDownMods = 0
                selectedApp = ""
                capturingUp = false
                capturingDown = false
                customStepSize = 2
                if (appComboBox.count > 0) {
                    appComboBox.currentIndex = 0
                }
            }

            onClosed: {
                KeyboardShortcutManager.globalShortcutsSuspended = false
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 15

                Label {
                    text: qsTr("Application")
                    font.bold: true
                }

                ComboBox {
                    id: appComboBox
                    Layout.fillWidth: true
                    model: AudioBridge.groupedApplications
                    textRole: "displayName"
                    valueRole: "executableName"
                    onCurrentIndexChanged: {
                        if (currentIndex >= 0) {
                            Qt.callLater(function() {
                                addHotkeyDialog.selectedApp = appComboBox.currentValue
                            })
                        }
                    }
                }

                Label {
                    text: qsTr("Volume Up Shortcut")
                    font.bold: true
                    Layout.topMargin: 5
                }

                Rectangle {
                    id: volUpRect
                    Layout.fillWidth: true
                    Layout.preferredHeight: 50
                    color: Constants.footerColor
                    radius: 5

                    Rectangle {
                        anchors.fill: parent
                        border.width: 1
                        border.color: addHotkeyDialog.capturingUp ? palette.accent : Constants.footerBorderColor
                        opacity: addHotkeyDialog.capturingUp ? 1 : 0.15
                        color: Constants.footerColor
                        radius: 5

                        Behavior on opacity {
                            NumberAnimation { duration: 200; easing.type: Easing.OutQuad }
                        }
                        Behavior on border.color {
                            ColorAnimation { duration: 200; easing.type: Easing.OutQuad }
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        text: addHotkeyDialog.volUpKey !== Qt.Key_unknown
                              ? Context.getShortcutText(addHotkeyDialog.volUpMods, addHotkeyDialog.volUpKey)
                              : qsTr("Click to set...")
                        font.family: "Consolas, monospace"
                        font.pixelSize: 14
                        opacity: addHotkeyDialog.volUpKey !== Qt.Key_unknown ? 1 : 0.5
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            addHotkeyDialog.capturingUp = true
                            addHotkeyDialog.capturingDown = false
                            volUpRect.forceActiveFocus()
                        }
                    }

                    Keys.onPressed: function(event) {
                        if (!addHotkeyDialog.capturingUp) return
                        let modifiers = 0
                        if (event.modifiers & Qt.ControlModifier) modifiers |= Qt.ControlModifier
                        if (event.modifiers & Qt.ShiftModifier) modifiers |= Qt.ShiftModifier
                        if (event.modifiers & Qt.AltModifier) modifiers |= Qt.AltModifier

                        addHotkeyDialog.volUpMods = modifiers
                        addHotkeyDialog.volUpKey = event.key
                        addHotkeyDialog.capturingUp = false
                        event.accepted = true
                    }
                }

                Label {
                    text: qsTr("Volume Down Shortcut")
                    font.bold: true
                    Layout.topMargin: 5
                }

                Rectangle {
                    id: volDownRect
                    Layout.fillWidth: true
                    Layout.preferredHeight: 50
                    color: Constants.footerColor
                    radius: 5

                    Rectangle {
                        anchors.fill: parent
                        border.width: 1
                        border.color: addHotkeyDialog.capturingDown ? palette.accent : Constants.footerBorderColor
                        opacity: addHotkeyDialog.capturingDown ? 1 : 0.15
                        color: Constants.footerColor
                        radius: 5

                        Behavior on opacity {
                            NumberAnimation { duration: 200; easing.type: Easing.OutQuad }
                        }
                        Behavior on border.color {
                            ColorAnimation { duration: 200; easing.type: Easing.OutQuad }
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        text: addHotkeyDialog.volDownKey !== Qt.Key_unknown
                              ? Context.getShortcutText(addHotkeyDialog.volDownMods, addHotkeyDialog.volDownKey)
                              : qsTr("Click to set...")
                        font.family: "Consolas, monospace"
                        font.pixelSize: 14
                        opacity: addHotkeyDialog.volDownKey !== Qt.Key_unknown ? 1 : 0.5
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            addHotkeyDialog.capturingDown = true
                            addHotkeyDialog.capturingUp = false
                            volDownRect.forceActiveFocus()
                        }
                    }

                    Keys.onPressed: function(event) {
                        if (!addHotkeyDialog.capturingDown) return
                        let modifiers = 0
                        if (event.modifiers & Qt.ControlModifier) modifiers |= Qt.ControlModifier
                        if (event.modifiers & Qt.ShiftModifier) modifiers |= Qt.ShiftModifier
                        if (event.modifiers & Qt.AltModifier) modifiers |= Qt.AltModifier

                        addHotkeyDialog.volDownMods = modifiers
                        addHotkeyDialog.volDownKey = event.key
                        addHotkeyDialog.capturingDown = false
                        event.accepted = true
                    }
                }

                Label {
                    text: qsTr("Volume Step Size")
                    font.bold: true
                    Layout.topMargin: 5
                }

                SpinBox {
                    id: stepSizeSpinBox
                    Layout.fillWidth: true
                    from: 1
                    to: 100
                    value: addHotkeyDialog.customStepSize
                    onValueChanged: addHotkeyDialog.customStepSize = value
                }

                RowLayout {
                    spacing: 15
                    Layout.topMargin: 10

                    Button {
                        text: qsTr("Cancel")
                        onClicked: addHotkeyDialog.close()
                        Layout.fillWidth: true
                    }

                    Button {
                        text: qsTr("Add")
                        highlighted: true
                        Layout.fillWidth: true
                        enabled: addHotkeyDialog.selectedApp !== "" &&
                                 addHotkeyDialog.volUpKey !== Qt.Key_unknown &&
                                 addHotkeyDialog.volDownKey !== Qt.Key_unknown
                        onClicked: {
                            KeyboardShortcutManager.addAppVolumeHotkey(
                                addHotkeyDialog.selectedApp,
                                addHotkeyDialog.volUpKey,
                                addHotkeyDialog.volUpMods,
                                addHotkeyDialog.volDownKey,
                                addHotkeyDialog.volDownMods,
                                addHotkeyDialog.customStepSize
                            )
                            addHotkeyDialog.close()
                        }
                    }
                }
            }
        }
    }
}
