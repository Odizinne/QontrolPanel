import QtQuick.Controls.FluentWinUI3
import QtQuick

Slider {
    id: slider
    focusPolicy: Qt.NoFocus

    signal wheelChanged()

    WheelHandler {
        onWheel: (event) => {
            let delta = event.angleDelta.y / 120
            slider.value += delta * UserSettings.sliderWheelSensivity
            slider.value = Math.max(slider.from, Math.min(slider.to, slider.value))

            slider.wheelChanged()
        }
    }
}
