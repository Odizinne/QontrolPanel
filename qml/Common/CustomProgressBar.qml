import QtQuick
import QtQuick.Controls.FluentWinUI3

Slider {
    id: control

    property bool loading: false

    handle: Item {
        implicitHeight: control.horizontal
        enabled: false
        visible: false
    }

    MouseArea {
        anchors.fill: parent
        enabled: true
        acceptedButtons: Qt.AllButtons
        preventStealing: true
        hoverEnabled: false
    }

    background: Item {
        implicitWidth: control.horizontal
            ? (control.__config.groove.width)
            : (control.__config.groove.height)
        implicitHeight: control.horizontal
            ? (control.__config.groove.height)
            : (control.__config.groove.width)

        property Item _background: Item {
            parent: control.background
            width: parent.width
            height: parent.height

            property Item groove: Item {
                parent: control.background._background
                x: control.leftPadding - control.leftInset + (control.horizontal
                    ? control.__config.handle.width / 2
                    : (control.availableWidth - width) / 2)
                y: control.topPadding - control.topInset + (control.horizontal
                    ? ((control.availableHeight - height) / 2)
                    : control.__config.handle.height / 2)

                width: control.horizontal
                    ? control.availableWidth - control.__config.handle.width
                    : 4
                height: control.horizontal
                    ? 4
                    : control.availableHeight - control.__config.handle.width

                Rectangle {
                    anchors.fill: parent
                    radius: control.__config.track.height * 0.5
                    color: control.palette.button
                }

                property Rectangle track: Rectangle {
                    parent: control.background._background.groove
                    y: control.horizontal ? 0 : parent.height - (parent.height * control.position)
                    width: control.horizontal ? parent.width * control.position : parent.width
                    height: control.horizontal ? parent.height : parent.height * control.position
                    radius: control.__config.track.height * 0.5
                    color: control.palette.accent
                    clip: true

                    // Loading shimmer effect
                    Rectangle {
                        visible: control.loading
                        width: control.horizontal ? parent.width * 0.5 : parent.width
                        height: control.horizontal ? parent.height : parent.height * 0.5
                        radius: parent.radius
                        gradient: Gradient {
                            orientation: control.horizontal ? Gradient.Horizontal : Gradient.Vertical
                            GradientStop { position: 0.0; color: "transparent" }
                            GradientStop { position: 0.5; color: Qt.lighter(control.palette.accent, 1.3) }
                            GradientStop { position: 1.0; color: "transparent" }
                        }

                        SequentialAnimation on x {
                            running: control.loading && control.horizontal
                            loops: Animation.Infinite
                            NumberAnimation {
                                from: -parent.width * 0.25
                                to: parent.width
                                duration: 3500  // Shimmer speed
                                easing.type: Easing.InOutQuad
                            }
                            PauseAnimation {
                                duration: 0  // Delay between loops
                            }
                        }

                        SequentialAnimation on y {
                            running: control.loading && !control.horizontal
                            loops: Animation.Infinite
                            NumberAnimation {
                                from: parent.height
                                to: -parent.height * 0.25
                                duration: 3500  // Shimmer speed
                                easing.type: Easing.InOutQuad
                            }
                            PauseAnimation {
                                duration: 0  // Delay between loops
                            }
                        }
                    }
                }
            }
        }
    }
}
