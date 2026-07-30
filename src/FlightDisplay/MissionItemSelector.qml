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

/// Fly View top-right jump box. Lists the mission items currently on the vehicle
/// by sequence number and command name; picking a different one commands the
/// vehicle to jump there (MAV_CMD_DO_SET_MISSION_CURRENT). Picking the item the
/// vehicle is already on — or just opening and closing the list — sends nothing.
Rectangle {
    id:     root
    width:  contentLayout.implicitWidth + _padding * 2
    height: contentLayout.implicitHeight + _padding * 2
    color:  qgcPal.toolbarBackground
    radius: ScreenTools.defaultFontPixelHeight / 2
    // Nothing to jump to without a vehicle or without a mission on it.
    visible: _activeVehicle && _entries.length > 0

    property var _activeVehicle:        QGroundControl.multiVehicleManager.activeVehicle
    property var _planMasterController: globals.planMasterControllerFlyView
    property var _missionController:    _planMasterController ? _planMasterController.missionController : null

    /// [{ seq: <mission sequence number>, text: "<seq>: <command name>" }, ...]
    property var  _entries: []
    property real _padding: ScreenTools.defaultFontPixelHeight / 2

    QGCPalette { id: qgcPal }

    // Keep clicks on the box from falling through to the map underneath.
    DeadMouseArea { anchors.fill: parent }

    function _rebuildEntries() {
        var list = []
        var visualItems = _missionController ? _missionController.visualItems : null
        if (visualItems) {
            for (var i = 0; i < visualItems.count; i++) {
                var item = visualItems.get(i)
                if (!item) {
                    continue
                }
                // Sequence 0 is the planned home position, not something the
                // vehicle can be told to fly to.
                if (item.sequenceNumber <= 0) {
                    continue
                }
                list.push({ seq: item.sequenceNumber,
                            text: item.sequenceNumber + ": " + item.commandName })
            }
        }
        _entries = list
        _syncToVehicle()
    }

    function _indexForSeq(seq) {
        for (var i = 0; i < _entries.length; i++) {
            if (_entries[i].seq === seq) {
                return i
            }
        }
        return -1
    }

    /// Follow the vehicle: the box always shows the item the vehicle is actually on,
    /// whether it got there by a jump from here or by finishing the previous item.
    function _syncToVehicle() {
        var index = _indexForSeq(_missionController ? _missionController.currentMissionIndex : -1)
        if (index >= 0 && index !== itemCombo.currentIndex) {
            itemCombo.currentIndex = index
        }
    }

    Component.onCompleted: _rebuildEntries()

    Connections {
        target:                         _missionController
        function onVisualItemsChanged()        { _rebuildEntries() }
        function onCurrentMissionIndexChanged() { _syncToVehicle() }
    }

    RowLayout {
        id:                 contentLayout
        anchors.centerIn:   parent
        spacing:            ScreenTools.defaultFontPixelWidth

        QGCLabel {
            text: qsTr("Mission item")
        }

        QGCComboBox {
            id:             itemCombo
            sizeToContents: true
            model:          _entries.map(function(entry) { return entry.text })

            // activated fires only on a real pick from the list, so opening and
            // closing the popup does nothing. Re-picking the item the vehicle is
            // already on is filtered out below, so the vehicle is never commanded
            // to jump to where it already is.
            onActivated: function(index) {
                if (index < 0 || index >= _entries.length) {
                    return
                }
                var seq = _entries[index].seq
                if (!_activeVehicle || seq === _missionController.currentMissionIndex) {
                    _syncToVehicle()
                    return
                }
                _activeVehicle.setCurrentMissionSequence(seq)
            }
        }
    }
}
