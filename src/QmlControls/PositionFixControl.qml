/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.Palette
import QGroundControl.ScreenTools

/// AURA: live position fix. Type the coordinate the vehicle is physically sitting on and
/// press the button; the navigation solution is snapped onto it (Vehicle::fixPositionEstimate
/// -> MAV_CMD_EXTERNAL_POSITION_ESTIMATE). Same operation as the MAV_CMD_AURA_POSITION_FIX
/// mission item, but issued now instead of from a plan, so it is the way to clear DVL drift
/// without editing and re-uploading the mission.
/// Deliberately a plain button, not a toggle: this is a one-shot correction, there is no
/// "fixing" state to stay in.
Rectangle {
    id:     root
    width:  contentLayout.implicitWidth + (showBackground ? _padding * 2 : 0)
    height: contentLayout.implicitHeight + (showBackground ? _padding * 2 : 0)
    color:  showBackground ? qgcPal.toolbarBackground : "transparent"
    radius: showBackground ? ScreenTools.defaultFontPixelHeight / 2 : 0

    /// Fly View draws this over the map, where it needs its own panel; the Plan toolbar
    /// already has one behind it.
    property bool showBackground: false

    property var  _activeVehicle:   QGroundControl.multiVehicleManager.activeVehicle
    property real _padding:         ScreenTools.defaultFontPixelHeight / 2
    property real _fieldWidth:      ScreenTools.defaultFontPixelWidth * 13

    // The boxes have no validator, so the operator's keyboard decides the decimal
    // separator. parseFloat("40,7493696") stops at the comma and returns 40 - a
    // coordinate hundreds of kilometres away. Normalise before parsing.
    function _number(text) {
        return parseFloat(String(text).trim().replace(",", "."))
    }

    property real _lat: _number(latField.text)
    property real _lon: _number(lonField.text)
    property bool _coordValid: !isNaN(_lat) && !isNaN(_lon) &&
                               _lat >= -90 && _lat <= 90 && _lon >= -180 && _lon <= 180

    QGCPalette { id: qgcPal }

    // Keeps clicks on the panel from falling through to the map underneath.
    DeadMouseArea { anchors.fill: parent; enabled: showBackground }

    RowLayout {
        id:                 contentLayout
        anchors.centerIn:   parent
        spacing:            ScreenTools.defaultFontPixelWidth

        QGCTextField {
            id:                     latField
            Layout.preferredWidth:  _fieldWidth
            // Placeholder shows where the vehicle currently thinks it is, so the operator
            // can see what is about to be overwritten.
            placeholderText:        _activeVehicle && _activeVehicle.coordinate.isValid
                                        ? _activeVehicle.coordinate.latitude.toFixed(7)
                                        : qsTr("Lat")
        }

        QGCTextField {
            id:                     lonField
            Layout.preferredWidth:  _fieldWidth
            placeholderText:        _activeVehicle && _activeVehicle.coordinate.isValid
                                        ? _activeVehicle.coordinate.longitude.toFixed(7)
                                        : qsTr("Lon")
        }

        QGCButton {
            text:       qsTr("Fix position")
            enabled:    _activeVehicle && _coordValid
            onClicked:  _activeVehicle.fixPositionEstimate(_lat, _lon)
        }
    }
}
