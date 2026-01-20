// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

import QtQuick
import QtQuick.Controls.FluentWinUI3

NFSlider {
    id: control
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
}
