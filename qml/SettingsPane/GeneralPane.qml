import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.FluentWinUI3
import Odizinne.QontrolPanel

ColumnLayout {
    spacing: 3

    Label {
        text: qsTr("General Settings")
        font.pixelSize: 22
        font.bold: true
        Layout.bottomMargin: 15
    }

    CustomScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        ColumnLayout {
            width: parent.width
            spacing: 3

            Card {
                Layout.fillWidth: true
                title: qsTr("Run at system startup")
                description: qsTr("QSS will boot up when your computer starts")

                additionalControl: LabeledSwitch {
                    checked: StartupShortcutBridge.getShortcutState()
                    onClicked: StartupShortcutBridge.setStartupShortcut(checked)
                }
            }

            Card {
                Layout.fillWidth: true
                title: qsTr("Show power action confirmation")
                additionalControl: LabeledSwitch {
                    checked: UserSettings.showPowerDialogConfirmation
                    onClicked: UserSettings.showPowerDialogConfirmation = checked
                }
            }

            Card {
                Layout.fillWidth: true
                title: qsTr("Slider wheel sensivity")
                additionalControl: SpinBox {
                    from: 1
                    to: 10
                    value: UserSettings.sliderWheelSensivity
                    onValueChanged: UserSettings.sliderWheelSensivity = value
                }
            }

            Card {
                Layout.fillWidth: true
                title: qsTr("DDC/CI brightness update rate")
                description: qsTr("Controls how frequently brightness commands are sent to external monitors")
                additionalControl: CustomComboBox {
                    Layout.preferredHeight: 35
                    model: [qsTr("Normal"), qsTr("Fast"), qsTr("Faster"), qsTr("Lightspeed")]
                    currentIndex: {
                        switch(UserSettings.ddcciQueueDelay) {
                            case 500: return 0
                            case 250: return 1
                            case 100: return 2
                            case 1: return 3
                            default: return 0
                        }
                    }
                    onActivated: {
                        switch(currentIndex) {
                            case 0: UserSettings.ddcciQueueDelay = 500; break
                            case 1: UserSettings.ddcciQueueDelay = 250; break
                            case 2: UserSettings.ddcciQueueDelay = 100; break
                            case 3: UserSettings.ddcciQueueDelay = 1; break
                        }
                    }
                }
            }
        }
    }
}
