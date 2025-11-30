import QtQuick.Controls.FluentWinUI3
import Odizinne.QontrolPanel

Menu {
    id: powerMenu

    signal setPowerAction(int action)

    MenuItem {
        enabled: PowerBridge.isSleepSupported()
        icon.source: "qrc:/icons/sleep.svg"
        text: qsTr("Sleep")
        onTriggered: PowerBridge.sleep()
    }

    MenuItem {
        icon.source: "qrc:/icons/hibernate.svg"
        text: qsTr("Hibernate")
        onTriggered: {
            if (UserSettings.showPowerDialogConfirmation) {
                powerMenu.setPowerAction(0)
            } else {
                PowerBridge.hibernate()
            }
        }
        enabled: PowerBridge.isHibernateSupported()
    }

    MenuItem {
        icon.source: "qrc:/icons/restart.svg"
        text: qsTr("Restart")
        onTriggered: {
            if (UserSettings.showPowerDialogConfirmation) {
                powerMenu.setPowerAction(1)
            } else {
                PowerBridge.restart()
            }
        }
    }

    MenuItem {
        icon.source: "qrc:/icons/restart.svg"
        text: qsTr("Restart UEFI")
        enabled: PowerBridge.isUEFISupported
        onTriggered: {
            if (UserSettings.showPowerDialogConfirmation) {
                powerMenu.setPowerAction(4)
            } else {
                PowerBridge.restartToUEFI()
            }
        }
    }

    MenuItem {
        icon.source: "qrc:/icons/shutdown.svg"
        text: qsTr("Shutdown")
        onTriggered: {
            if (UserSettings.showPowerDialogConfirmation) {
                powerMenu.setPowerAction(2)
            } else {
                PowerBridge.shutdown()
            }
        }
    }

    MenuSeparator {}

    MenuItem {
        icon.source: "qrc:/icons/lock.svg"
        text: qsTr("Lock")
        onTriggered: PowerBridge.lockAccount()
    }

    MenuItem {
        icon.source: "qrc:/icons/signout.svg"
        text: qsTr("Sign Out")
        onTriggered: {
            if (UserSettings.showPowerDialogConfirmation) {
                powerMenu.setPowerAction(3)
            } else {
                PowerBridge.signOut()
            }
        }
    }

    MenuItem {
        icon.source: "qrc:/icons/switch.svg"
        enabled: PowerBridge.hasMultipleUsers()
        text: qsTr("Switch User")
        onTriggered: PowerBridge.switchAccount()
    }
}
