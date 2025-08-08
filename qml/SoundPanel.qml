pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.FluentWinUI3
import QtQuick.Controls.impl
import QtQuick.Window
import Odizinne.QuickSoundSwitcher
import QtMultimedia

ApplicationWindow {
    id: panel
    width: 360
    height: {
        let baseMargins = 30
        let newHeight = mainLayout.implicitHeight + baseMargins

        if (mediaLayout.visible) {
            newHeight += mediaLayout.implicitHeight
            newHeight += spacer.height
        }

        newHeight += panel.maxDeviceListSpace
        return newHeight
    }

    signal globalShortcutsToggled(bool enabled)

    Component.onCompleted: {
        updateAudioFeedbackDevice()
        SoundPanelBridge.startMediaMonitoring()
    }

    Connections {
        target: UserSettings
        function onOpacityAnimationsChanged() {
            if (panel.visible) return

            if (UserSettings.opacityAnimations) {
                mediaLayout.opacity = 0
                mainLayout.opacity = 0
            } else {
                mediaLayout.opacity = 1
                mainLayout.opacity = 1
            }
        }

        function onGlobalShortcutsEnabledChanged() {
            panel.globalShortcutsToggled(UserSettings.globalShortcutsEnabled)
        }
    }

    MediaPlayer {
        id: audioFeedback
        source: "qrc:/sounds/windows-background.wav"
        audioOutput: AudioOutput {
            volume: 1
            device: mediaDevices.defaultAudioOutput
        }
    }

    MediaDevices {
        id: mediaDevices
        onAudioOutputsChanged: {
            panel.updateAudioFeedbackDevice()
        }
    }

    function updateAudioFeedbackDevice() {
        const device = mediaDevices.defaultAudioOutput
        audioFeedback.audioOutput.device = device

        if (audioFeedback.playbackState === MediaPlayer.PlayingState) {
            audioFeedback.stop()
            audioFeedback.play()
        }
    }

    SystemTray {
        onTogglePanelRequested: {
            if (panel.visible) {
                panel.hidePanel()
            } else {
                panel.showPanel()
            }
        }
    }
    MouseArea {
        height: panel.maxDeviceListSpace - panel.currentUsedListSpace
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: UserSettings.panelPosition === 0 ? parent.bottom : undefined
        anchors.top: UserSettings.panelPosition === 0 ? undefined : parent.top
        onClicked: panel.hidePanel()
    }

    Shortcut {
        sequence: StandardKey.Cancel
        onActivated: {
            if (!panel.isAnimatingIn && !panel.isAnimatingOut && panel.visible) {
                panel.hidePanel()
            }
        }
    }

    visible: false
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "#00000000"
    property bool isAnimatingIn: false
    property bool isAnimatingOut: false
    property string taskbarPos: SoundPanelBridge.taskbarPosition

    property real maxDeviceListSpace: {
        let outputSpace = outputDevicesRect.expandedNeededHeight || 0
        let inputSpace = inputDevicesRect.expandedNeededHeight || 0

        return outputSpace + inputSpace
    }

    property real currentUsedListSpace: {
        let usedSpace = 0
        if (outputDevicesRect.expanded) {
            usedSpace += outputDevicesRect.expandedNeededHeight || 0
        }
        if (inputDevicesRect.expanded) {
            usedSpace += inputDevicesRect.expandedNeededHeight || 0
        }
        return usedSpace
    }

    property real listCompensationOffset: maxDeviceListSpace - currentUsedListSpace

    signal hideAnimationFinished()
    signal showAnimationFinished()
    signal hideAnimationStarted()

    ChatMixNotification {}

    onVisibleChanged: {
        if (!visible) {
            if (UserSettings.opacityAnimations) {
                mediaLayout.opacity = 0
                mainLayout.opacity = 0
            }

            if (UserSettings.showAudioLevel) {
                AudioBridge.stopAudioLevelMonitoring()
                AudioBridge.stopApplicationAudioLevelMonitoring() // Add this
            }
        } else {
            if (UserSettings.showAudioLevel) {
                AudioBridge.startAudioLevelMonitoring()
                AudioBridge.startApplicationAudioLevelMonitoring() // Add this
            }
        }
    }

    Timer {
        id: contentOpacityTimer
        interval: 160
        repeat: false
        onTriggered: mainLayout.opacity = 1
    }

    Timer {
        id: flyoutOpacityTimer
        interval: 160
        repeat: false
        onTriggered: mediaLayout.opacity = 1
    }

    onHeightChanged: {
        if (isAnimatingIn && !isAnimatingOut) {
            positionPanelAtTarget()
        }
    }

    PropertyAnimation {
        id: showAnimation
        target: contentTransform
        duration: 180
        easing.type: Easing.OutCubic
        onStarted: {
            contentOpacityTimer.start()
            flyoutOpacityTimer.start()
        }
        onFinished: {
            panel.isAnimatingIn = false
            panel.showAnimationFinished()
        }
    }

    PropertyAnimation {
        id: hideAnimation
        target: contentTransform
        duration: 180
        easing.type: Easing.InCubic
        onFinished: {
            panel.visible = false
            panel.isAnimatingOut = false
            panel.hideAnimationFinished()
        }
    }

    Translate {
        id: contentTransform
        property real x: 0
        property real y: 0
    }

    Translate {
        id: listCompensationTransform
        x: 0
        y: (panel.isAnimatingIn || panel.isAnimatingOut) ? 0 : -panel.listCompensationOffset

        Behavior on y {
            enabled: !panel.isAnimatingIn && !panel.isAnimatingOut
            NumberAnimation {
                duration: 150
                easing.type: Easing.OutQuad
            }
        }
    }

    function showPanel() {
        if (isAnimatingIn || isAnimatingOut) {
            return
        }

        isAnimatingIn = true
        Constants.darkMode = SoundPanelBridge.getDarkMode()
        panel.taskbarPos = SoundPanelBridge.taskbarPosition
        panel.visible = true

        positionPanelAtTarget()
        setInitialTransform()

        Qt.callLater(function() {
            Qt.callLater(function() {
                let newHeight = mainLayout.implicitHeight + 30

                if (mediaLayout.visible) {
                    newHeight += mediaLayout.implicitHeight
                }

                if (spacer.visible) {
                    newHeight += spacer.height
                }

                newHeight += panel.maxDeviceListSpace

                let appListView = 0
                for (let i = 0; i < appRepeater.count; ++i) {
                    let item = appRepeater.itemAt(i)
                    if (item && item.hasOwnProperty('applicationListHeight')) {
                        appListView += item.applicationListHeight || 0
                    }
                }

                newHeight += appListView

                panel.height = newHeight
                Qt.callLater(panel.startAnimation)
            })
        })
    }

    function positionPanelAtTarget() {
        const screenWidth = Qt.application.screens[0].width
        const screenHeight = Qt.application.screens[0].height

        switch (panel.taskbarPos) {
        case "top":
            panel.x = screenWidth - width - UserSettings.xAxisMargin
            panel.y = UserSettings.yAxisMargin + UserSettings.taskbarOffset
            break
        case "bottom":
            panel.x = screenWidth - width - UserSettings.xAxisMargin
            panel.y = screenHeight - height - UserSettings.yAxisMargin - UserSettings.taskbarOffset
            break
        case "left":
            panel.x = UserSettings.taskbarOffset + UserSettings.xAxisMargin
            panel.y = screenHeight - height - UserSettings.yAxisMargin
            break
        case "right":
            panel.x = screenWidth - width - UserSettings.xAxisMargin - UserSettings.taskbarOffset
            panel.y = screenHeight - height - UserSettings.yAxisMargin
            break
        default:
            panel.x = screenWidth - width - UserSettings.xAxisMargin
            panel.y = screenHeight - height - UserSettings.yAxisMargin - UserSettings.taskbarOffset
            break
        }
    }

    function setInitialTransform() {
        switch (panel.taskbarPos) {
        case "top":
            contentTransform.y = -height
            contentTransform.x = 0
            break
        case "bottom":
            contentTransform.y = height
            contentTransform.x = 0
            break
        case "left":
            contentTransform.x = -width
            contentTransform.y = 0
            break
        case "right":
            contentTransform.x = width
            contentTransform.y = 0
            break
        default:
            contentTransform.y = height
            contentTransform.x = 0
            break
        }
    }

    function startAnimation() {
        if (!isAnimatingIn) return

        showAnimation.properties = panel.taskbarPos === "left" || panel.taskbarPos === "right" ? "x" : "y"
        showAnimation.from = panel.taskbarPos === "left" || panel.taskbarPos === "right" ? contentTransform.x : contentTransform.y
        showAnimation.to = 0
        showAnimation.start()
    }

    function hidePanel() {
        if (isAnimatingIn || isAnimatingOut) {
            return
        }

        if (executableRenameContextMenu.visible) {
            executableRenameContextMenu.close()
        }

        // Close context menus in application lists
        for (let i = 0; i < appRepeater.count; ++i) {
            let item = appRepeater.itemAt(i)
            if (item && item.children) {
                for (let j = 0; j < item.children.length; ++j) {
                    let child = item.children[j]
                    if (child && child.hasOwnProperty('closeContextMenus')) {
                        child.closeContextMenus()
                    }
                }
            }
        }

        outputDevicesRect.expanded = false
        inputDevicesRect.expanded = false

        for (let i = 0; i < appRepeater.count; ++i) {
            let item = appRepeater.itemAt(i)
            if (item && item.children) {
                for (let j = 0; j < item.children.length; ++j) {
                    let child = item.children[j]
                    if (child && child.hasOwnProperty('expanded')) {
                        child.expanded = false
                    }
                }
            }
        }

        isAnimatingOut = true
        panel.hideAnimationStarted()

        switch (panel.taskbarPos) {
        case "top":
            hideAnimation.properties = "y"
            hideAnimation.from = contentTransform.y
            hideAnimation.to = -height
            break
        case "bottom":
            hideAnimation.properties = "y"
            hideAnimation.from = contentTransform.y
            hideAnimation.to = height
            break
        case "left":
            hideAnimation.properties = "x"
            hideAnimation.from = contentTransform.x
            hideAnimation.to = -width
            break
        case "right":
            hideAnimation.properties = "x"
            hideAnimation.from = contentTransform.x
            hideAnimation.to = width
            break
        default:
            hideAnimation.properties = "y"
            hideAnimation.from = contentTransform.y
            hideAnimation.to = height
            break
        }

        hideAnimation.start()
    }

    Connections {
        target: SoundPanelBridge
        function onChatMixEnabledChanged(enabled) {
            UserSettings.chatMixEnabled = enabled

            if (enabled) {
                AudioBridge.applyChatMixToApplications(UserSettings.chatMixValue)
            } else {
                AudioBridge.restoreOriginalVolumes()
            }
        }
    }

    KeepAlive {}

    SettingsWindow {
        id: settingsWindow
    }

    Item {
        id: cont
        anchors.bottom: UserSettings.panelPosition === 0 ? undefined : parent.bottom
        anchors.top: UserSettings.panelPosition === 0 ? parent.top : undefined
        anchors.right: parent.right
        anchors.left: parent.left
        transform: Translate {
            x: contentTransform.x
            y: contentTransform.y
        }

        height: {
            let baseMargins = 30
            let newHeight = mainLayout.implicitHeight + baseMargins

            if (mediaLayout.visible) {
                newHeight += mediaLayout.implicitHeight
                newHeight += spacer.height
            }

            return newHeight
        }

        Rectangle {
            anchors.fill: mainLayout
            anchors.margins: -15
            color: Constants.panelColor
            radius: 12
            Rectangle {
                anchors.fill: parent
                color: "#00000000"
                radius: 12
                border.width: 1
                border.color: "#E3E3E3"
                opacity: 0.2
            }
        }

        Rectangle {
            id: mediaLayoutBackground
            anchors.fill: mediaLayout
            anchors.margins: -15
            color: Constants.panelColor
            visible: mediaLayout.visible
            radius: 12
            opacity: 0

            onVisibleChanged: {
                if (visible) {
                    fadeInAnimation.start()
                } else {
                    fadeOutAnimation.start()
                }
            }

            PropertyAnimation {
                id: fadeInAnimation
                target: mediaLayoutBackground
                property: "opacity"
                from: 0
                to: 1
                duration: 300
                easing.type: Easing.OutQuad
            }

            PropertyAnimation {
                id: fadeOutAnimation
                target: mediaLayoutBackground
                property: "opacity"
                from: 1
                to: 0
                duration: 300
                easing.type: Easing.OutQuad
            }

            Rectangle {
                anchors.fill: parent
                color: "#00000000"
                radius: 12
                border.width: 1
                border.color: "#E3E3E3"
                opacity: 0.2
            }
        }

        MediaFlyoutContent {
            id: mediaLayout
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 15
            anchors.leftMargin: 15
            anchors.rightMargin: 15
            anchors.bottomMargin: 0
            opacity: 0
            visible: UserSettings.mediaMode === 0 && (SoundPanelBridge.mediaTitle !== "")
            onVisibleChanged: {
                if (panel.visible) {
                    opacity = 1
                }
            }

            Behavior on opacity {
                NumberAnimation {
                    duration: 300
                    easing.type: Easing.OutQuad
                }
            }
        }

        Item {
            id: spacer
            anchors.top: mediaLayout.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 42
            visible: mediaLayout.visible
            //Rectangle {
            //    anchors.fill: parent
            //    color: "transparent"
            //    border.color: "red"
            //    border.width: 1
            //}
        }

        ColumnLayout {
            id: mainLayout
            anchors.top: mediaLayout.visible ? spacer.bottom : parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 15
            anchors.rightMargin: 15
            anchors.bottomMargin: 15
            anchors.topMargin: mediaLayout.visible ? 0 : 15  // Remove top margin when media flyout is visible
            spacing: 10
            opacity: 0

            Behavior on opacity {
                NumberAnimation {
                    duration: 300
                    easing.type: Easing.OutQuad
                }
            }

            ColumnLayout {
                id: deviceLayout
                spacing: 5
                visible: UserSettings.panelMode === 0 || UserSettings.panelMode === 2

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    spacing: 0
                    ToolButton {
                        Layout.preferredHeight: 40
                        Layout.preferredWidth: 40
                        flat: true
                        property string volumeIcon: {
                            if (AudioBridge.outputMuted || AudioBridge.outputVolume === 0) {
                                return "qrc:/icons/panel_volume_0.svg"
                            } else if (AudioBridge.outputVolume <= 33) {
                                return "qrc:/icons/panel_volume_33.svg"
                            } else if (AudioBridge.outputVolume <= 66) {
                                return "qrc:/icons/panel_volume_66.svg"
                            } else {
                                return "qrc:/icons/panel_volume_100.svg"
                            }
                        }

                        icon.source: volumeIcon
                        onClicked: {
                            AudioBridge.setOutputMute(!AudioBridge.outputMuted)
                        }
                    }

                    ColumnLayout {
                        spacing: -4
                        Label {
                            visible: UserSettings.displayDevAppLabel
                            opacity: 0.5
                            elide: Text.ElideRight
                            Layout.preferredWidth: outputSlider.implicitWidth - 30
                            Layout.leftMargin: 18
                            Layout.rightMargin: 25
                            text: {
                                if (!AudioBridge.isReady) return ""

                                let defaultIndex = AudioBridge.outputDevices.currentDefaultIndex
                                if (defaultIndex >= 0) {
                                    if (UserSettings.deviceShortName) {
                                        return AudioBridge.outputDevices.getDeviceShortName(defaultIndex)
                                    } else {
                                        return AudioBridge.outputDevices.getDeviceName(defaultIndex)
                                    }
                                }
                                return ""
                            }
                        }

                        ProgressSlider {
                            id: outputSlider
                            value: pressed ? value : AudioBridge.outputVolume
                            from: 0
                            to: 100
                            Layout.fillWidth: true
                            audioLevel: AudioBridge.outputAudioLevel

                            //ToolTip {
                            //    parent: outputSlider.handle
                            //    visible: outputSlider.pressed
                            //    text: Math.round(outputSlider.value).toString()
                            //}

                            onValueChanged: {
                                if (pressed) {
                                    AudioBridge.setOutputVolume(value)
                                }
                            }

                            onPressedChanged: {
                                if (!pressed) {
                                    AudioBridge.setOutputVolume(value)
                                    audioFeedback.play()
                                }
                            }
                        }
                    }

                    ToolButton {
                        icon.source: "qrc:/icons/arrow.svg"
                        rotation: outputDevicesRect.expanded ? 90 : 0
                        visible: AudioBridge.isReady && AudioBridge.outputDevices.rowCount() > 1
                        Layout.preferredHeight: 35
                        Layout.preferredWidth: 35
                        Behavior on rotation {
                            NumberAnimation {
                                duration: 150
                                easing.type: Easing.Linear
                            }
                        }

                        onClicked: {
                            outputDevicesRect.expanded = !outputDevicesRect.expanded
                        }
                    }
                }

                DevicesListView {
                    id: outputDevicesRect
                    model: AudioBridge.outputDevices
                    onDeviceClicked: function(name, shortName, index) {
                        AudioBridge.setOutputDevice(index)

                        if (UserSettings.linkIO) {
                            // Try to find matching input device using shortName
                            let inputModel = AudioBridge.inputDevices
                            for (let i = 0; i < inputModel.rowCount(); ++i) {
                                let inputDeviceShortName = inputModel.getDeviceShortName(i)
                                if (inputDeviceShortName === shortName) {
                                    AudioBridge.setInputDevice(i)
                                    break
                                }
                            }
                        }
                        if (UserSettings.closeDeviceListOnClick) {
                            expanded = false
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    spacing: 0
                    ToolButton {
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        flat: true
                        icon.source: AudioBridge.inputMuted ? "qrc:/icons/mic_muted.svg" : "qrc:/icons/mic.svg"
                        icon.height: 16
                        icon.width: 16
                        onClicked: {
                            AudioBridge.setInputMute(!AudioBridge.inputMuted)
                        }
                    }

                    ColumnLayout {
                        spacing: -4
                        Label {
                            visible: UserSettings.displayDevAppLabel
                            opacity: 0.5
                            elide: Text.ElideRight
                            Layout.preferredWidth: inputSlider.implicitWidth - 30
                            Layout.leftMargin: 18
                            Layout.rightMargin: 25
                            text: {
                                if (!AudioBridge.isReady) return ""

                                let defaultIndex = AudioBridge.inputDevices.currentDefaultIndex
                                if (defaultIndex >= 0) {
                                    if (UserSettings.deviceShortName) {
                                        return AudioBridge.inputDevices.getDeviceShortName(defaultIndex)
                                    } else {
                                        return AudioBridge.inputDevices.getDeviceName(defaultIndex)
                                    }
                                }
                                return ""
                            }
                        }

                        ProgressSlider {
                            id: inputSlider
                            value: pressed ? value : AudioBridge.inputVolume
                            from: 0
                            to: 100
                            audioLevel: AudioBridge.inputAudioLevel
                            Layout.fillWidth: true

                            //ToolTip {
                            //    parent: inputSlider.handle
                            //    visible: inputSlider.pressed
                            //    text: Math.round(inputSlider.value).toString()
                            //}

                            onValueChanged: {
                                if (pressed) {
                                    AudioBridge.setInputVolume(value)
                                }
                            }

                            onPressedChanged: {
                                if (!pressed) {
                                    AudioBridge.setInputVolume(value)
                                }
                            }
                        }
                    }

                    ToolButton {
                        icon.source: "qrc:/icons/arrow.svg"
                        rotation: inputDevicesRect.expanded ? 90 : 0
                        Layout.preferredHeight: 35
                        Layout.preferredWidth: 35
                        visible: AudioBridge.isReady && AudioBridge.inputDevices.rowCount() > 1
                        Behavior on rotation {
                            NumberAnimation {
                                duration: 150
                                easing.type: Easing.Linear
                            }
                        }

                        onClicked: {
                            inputDevicesRect.expanded = !inputDevicesRect.expanded
                        }
                    }
                }

                DevicesListView {
                    id: inputDevicesRect
                    model: AudioBridge.inputDevices
                    onDeviceClicked: function(name, shortName, index) {
                        AudioBridge.setInputDevice(index)
                        if (UserSettings.linkIO) {
                            // Try to find matching output device using shortName
                            let outputModel = AudioBridge.outputDevices
                            for (let i = 0; i < outputModel.rowCount(); ++i) {
                                let outputDeviceShortName = outputModel.getDeviceShortName(i)
                                if (outputDeviceShortName === shortName) {
                                    AudioBridge.setOutputDevice(i)
                                    break
                                }
                            }
                        }
                        if (UserSettings.closeDeviceListOnClick) {
                            expanded = false
                        }
                    }
                }
            }

            Rectangle {
                Layout.preferredHeight: 1
                Layout.fillWidth: true
                color: Constants.separatorColor
                opacity: 0.15
                visible: UserSettings.panelMode === 0 && AudioBridge.isReady && AudioBridge.applications.rowCount() > 0
                Layout.rightMargin: -14
                Layout.leftMargin: -14
            }

            ColumnLayout {
                id: appLayout
                spacing: 5
                visible: UserSettings.panelMode < 2 && AudioBridge.isReady && AudioBridge.applications.rowCount() > 0

                Repeater {
                    id: appRepeater
                    model: AudioBridge.groupedApplications // You'll need to implement this model

                    delegate: ColumnLayout {
                        id: appDelegateRoot
                        spacing: 5
                        Layout.fillWidth: true
                        required property var model
                        required property int index

                        readonly property real applicationListHeight: individualAppsRect.expandedNeededHeight

                        // Main application row (represents the executable)
                        RowLayout {
                            Layout.preferredHeight: 40
                            Layout.fillWidth: true
                            spacing: 0

                            ToolButton {
                                id: executableMuteButton
                                Layout.preferredWidth: 40
                                Layout.preferredHeight: 40
                                flat: !checked
                                checkable: true
                                highlighted: checked
                                checked: appDelegateRoot.model.allMuted // Property from your grouped model
                                ToolTip.text: appDelegateRoot.model.displayName
                                ToolTip.visible: hovered
                                ToolTip.delay: 1000
                                opacity: highlighted ? 0.3 : (enabled ? 1 : 0.5)
                                icon.color: "transparent"
                                icon.source: appDelegateRoot.model.displayName === qsTr("System sounds") ? Constants.systemIcon : appDelegateRoot.model.iconPath

                                onClicked: {
                                    // Mute/unmute all sessions for this executable
                                    AudioBridge.setExecutableMute(appDelegateRoot.model.executableName, checked)
                                }

                                Component.onCompleted: {
                                    palette.accent = palette.button
                                }
                            }

                            ColumnLayout {
                                spacing: -4

                                Label {
                                    visible: UserSettings.displayDevAppLabel
                                    opacity: UserSettings.chatMixEnabled ? 0.3 : 0.5
                                    elide: Text.ElideRight
                                    Layout.preferredWidth: 200
                                    Layout.leftMargin: 18
                                    Layout.rightMargin: 25
                                    text: {
                                        let name = appDelegateRoot.model.displayName
                                        if (UserSettings.chatMixEnabled && AudioBridge.isCommApp(name)) {
                                            name += " (Comm)"
                                        }
                                        return name
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        acceptedButtons: Qt.RightButton
                                        onClicked: function(mouse) {
                                            if (mouse.button === Qt.RightButton && appDelegateRoot.model.executableName !== "System sounds") {
                                                executableRenameContextMenu.originalName = appDelegateRoot.model.executableName
                                                executableRenameContextMenu.currentCustomName = AudioBridge.getCustomExecutableName(appDelegateRoot.model.executableName)
                                                executableRenameContextMenu.popup()
                                            }
                                        }
                                    }
                                }

                                ProgressSlider {
                                    onActiveFocusChanged: {
                                        focus = false
                                    }
                                    id: executableVolumeSlider
                                    from: 0
                                    to: 100
                                    enabled: !UserSettings.chatMixEnabled && !executableMuteButton.highlighted
                                    opacity: enabled ? 1 : 0.5
                                    Layout.fillWidth: true
                                    displayProgress: appDelegateRoot.model.displayName !== qsTr("System sounds")
                                    audioLevel: appDelegateRoot.model.displayName !== qsTr("System sounds")
                                               ? (appDelegateRoot.model.averageAudioLevel || 0)
                                               : 0

                                    // Break the binding loop by only updating when not being dragged
                                    value: pressed ? value : appDelegateRoot.model.averageVolume

                                    //ToolTip {
                                    //    parent: executableVolumeSlider.handle
                                    //    visible: executableVolumeSlider.pressed
                                    //    text: Math.round(executableVolumeSlider.value).toString()
                                    //}

                                    onValueChanged: {
                                        if (!UserSettings.chatMixEnabled && pressed) {
                                            // Set volume for all sessions of this executable
                                            AudioBridge.setExecutableVolume(appDelegateRoot.model.executableName, value)
                                        }
                                    }

                                    onPressedChanged: {
                                        if (!pressed && !UserSettings.chatMixEnabled) {
                                            // Final update when releasing
                                            AudioBridge.setExecutableVolume(appDelegateRoot.model.executableName, value)
                                        }
                                    }
                                }
                            }

                            ToolButton {
                                onActiveFocusChanged: {
                                    focus = false
                                }

                                icon.source: "qrc:/icons/arrow.svg"
                                rotation: individualAppsRect.expanded ? 90 : 0
                                visible: appDelegateRoot.model.sessionCount > 1 // Only show if multiple sessions
                                Layout.preferredHeight: 35
                                Layout.preferredWidth: 35

                                Behavior on rotation {
                                    NumberAnimation {
                                        duration: 150
                                        easing.type: Easing.Linear
                                    }
                                }

                                onClicked: {
                                    individualAppsRect.expanded = !individualAppsRect.expanded
                                }
                            }
                        }

                        // Individual applications list
                        ApplicationsListView {
                            id: individualAppsRect
                            model: AudioBridge.getSessionsForExecutable(appDelegateRoot.model.executableName)
                            executableName: appDelegateRoot.model.executableName

                            onApplicationVolumeChanged: function(appId, volume) {
                                AudioBridge.setApplicationVolume(appId, volume)
                            }

                            onApplicationMuteChanged: function(appId, muted) {
                                AudioBridge.setApplicationMute(appId, muted)
                            }
                        }
                    }
                }
            }

            Rectangle {
                visible: UserSettings.activateChatmix
                Layout.preferredHeight: 1
                Layout.fillWidth: true
                color: Constants.separatorColor
                opacity: 0.15
                Layout.rightMargin: -14
                Layout.leftMargin: -14
            }

            ColumnLayout {
                visible: UserSettings.activateChatmix
                id: chatMixLayout
                spacing: 5

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    spacing: 0

                    ToolButton {
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        icon.width: 15
                        icon.height: 15
                        icon.source: "qrc:/icons/headset.svg"
                        icon.color: palette.text
                        checkable: true
                        checked: !UserSettings.chatMixEnabled
                        opacity: checked ? 0.3 : 1

                        Component.onCompleted: {
                            palette.accent = palette.button
                        }

                        onClicked: {
                            UserSettings.chatMixEnabled = !checked

                            if (!checked) {
                                AudioBridge.applyChatMixToApplications(UserSettings.chatMixValue)
                            } else {
                                AudioBridge.restoreOriginalVolumes()
                            }
                        }
                    }

                    ColumnLayout {
                        spacing: -4

                        Label {
                            visible: UserSettings.displayDevAppLabel
                            opacity: 0.5
                            text: qsTr("ChatMix")
                            Layout.leftMargin: 18
                            Layout.rightMargin: 25
                        }

                        Slider {
                            id: chatMixSlider
                            value: UserSettings.chatMixValue
                            from: 0
                            to: 100
                            Layout.fillWidth: true
                            enabled: UserSettings.chatMixEnabled

                            ToolTip {
                                parent: chatMixSlider.handle
                                visible: chatMixSlider.pressed || chatMixSlider.hovered
                                delay: chatMixSlider.pressed ? 0 : 1000
                                text: Math.round(chatMixSlider.value).toString()
                            }

                            onValueChanged: {
                                UserSettings.chatMixValue = value
                                if (UserSettings.chatMixEnabled) {
                                    AudioBridge.applyChatMixToApplications(Math.round(value))
                                }
                            }
                        }
                    }

                    IconImage {
                        Layout.preferredWidth: 35
                        Layout.preferredHeight: 35
                        sourceSize.width: 15
                        sourceSize.height: 15
                        color: palette.text
                        source: "qrc:/icons/music.svg"
                        enabled: UserSettings.chatMixEnabled
                    }
                }
            }

            Rectangle {
                color: Constants.footerColor
                Layout.fillWidth: true
                Layout.fillHeight: false
                bottomLeftRadius: 12
                bottomRightRadius: 12
                Layout.preferredHeight: 50
                Layout.leftMargin: -14
                Layout.rightMargin: -14
                Layout.bottomMargin: -14

                Rectangle {
                    height: 1
                    color: Constants.footerBorderColor
                    opacity: 0.15
                    visible: UserSettings.panelMode === 0
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.left: parent.left
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 10

                    Image {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        Layout.alignment: Qt.AlignVCenter
                        source: SoundPanelBridge.mediaArt || ""
                        fillMode: Image.PreserveAspectCrop
                        visible: SoundPanelBridge.mediaArt !== "" && UserSettings.mediaMode === 1
                    }

                    ColumnLayout {
                        spacing: 2
                        Layout.fillWidth: true
                        Item {
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                        }

                        Label {
                            text: SoundPanelBridge.mediaTitle || ""
                            font.pixelSize: 14
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            visible: UserSettings.mediaMode === 1
                        }

                        Label {
                            text: SoundPanelBridge.mediaArtist || ""
                            font.pixelSize: 12
                            opacity: 0.7
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            visible: UserSettings.mediaMode === 1
                        }

                        Item {
                            Layout.fillHeight: true
                        }
                    }

                    ToolButton {
                        icon.source: "qrc:/icons/settings.svg"
                        icon.width: 14
                        icon.height: 14
                        antialiasing: true
                        onClicked: {
                            settingsWindow.show()
                            panel.hidePanel()
                        }
                    }
                }
            }
        }
    }

    Menu {
        id: executableRenameContextMenu

        property string originalName: ""
        property string currentCustomName: ""

        MenuItem {
            text: qsTr("Rename Application")
            onTriggered: executableRenameDialog.open()
        }

        MenuItem {
            text: qsTr("Reset to Original Name")
            enabled: executableRenameContextMenu.currentCustomName !== executableRenameContextMenu.originalName
            onTriggered: {
                AudioBridge.setCustomExecutableName(executableRenameContextMenu.originalName, "")
            }
        }
    }

    // Rename dialog for executables
    Dialog {
        id: executableRenameDialog
        title: qsTr("Rename Application")
        modal: true
        width: 300
        dim: false
        anchors.centerIn: parent

        ColumnLayout {
            anchors.fill: parent
            spacing: 15

            Label {
                text: qsTr("Original name: ") + executableRenameContextMenu.originalName
                font.bold: true
            }

            Label {
                text: qsTr("Custom name:")
            }

            TextField {
                id: executableCustomNameField
                Layout.fillWidth: true
                placeholderText: executableRenameContextMenu.originalName

                Keys.onReturnPressed: {
                    AudioBridge.setCustomExecutableName(
                        executableRenameContextMenu.originalName,
                        executableCustomNameField.text.trim()
                    )
                    executableRenameDialog.close()
                }
            }

            RowLayout {
                spacing: 15
                Layout.topMargin: 10

                Button {
                    text: qsTr("Cancel")
                    onClicked: executableRenameDialog.close()
                    Layout.fillWidth: true
                }

                Button {
                    text: qsTr("Save")
                    highlighted: true
                    Layout.fillWidth: true
                    onClicked: {
                        AudioBridge.setCustomExecutableName(
                            executableRenameContextMenu.originalName,
                            executableCustomNameField.text.trim()
                        )
                        executableRenameDialog.close()
                    }
                }
            }
        }

        onOpened: {
            executableCustomNameField.text = executableRenameContextMenu.currentCustomName
            executableCustomNameField.forceActiveFocus()
            executableCustomNameField.selectAll()
        }
    }
}
