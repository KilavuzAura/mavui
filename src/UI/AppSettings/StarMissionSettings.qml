/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

import QGroundControl
import QGroundControl.FactSystem
import QGroundControl.FactControls
import QGroundControl.Controls
import QGroundControl.ScreenTools

// Tuning panel for the AUV Stars 2026 Mission One plan creator. Every value here
// is a SettingsFact, so it is written to QSettings the moment it is edited and
// comes back on the next start.
SettingsPage {
    property var _starMissionSettings: QGroundControl.settingsManager.starMissionSettings

    // A settings row plus the one-line explanation of what the value actually does.
    component DescribedField: ColumnLayout {
        property alias  fact:        _field.fact
        property alias  label:       _field.label
        property string description

        Layout.fillWidth:   true
        spacing:            0

        LabelledFactTextField {
            id:                 _field
            Layout.fillWidth:   true
        }

        QGCLabel {
            Layout.fillWidth:   true
            text:               description
            wrapMode:           Text.WordWrap
            font.pointSize:     ScreenTools.smallFontPointSize
        }
    }

    SettingsGroupLayout {
        Layout.fillWidth:   true
        heading:            qsTr("Drop Anchor")
        headingDescription: qsTr("MAV_CMD_AURA_ANCHOR (31010). The vehicle locks the point it stopped at and will not move on until it has held it. The camera work is part of the command: settle, turn to the heading, wait, fire, hold. Requires the aurapilot ArduSub fork.")

        DescribedField {
            label:       qsTr("Settle radius")
            fact:        _starMissionSettings.anchorRadius
            description: qsTr("How close to the locked point the vehicle counts as \"on station\". Set it smaller than the vehicle's real hold accuracy and it never counts as settled, so every anchor runs on to its guard timeout. 0 turns the settle check off and the anchor becomes a plain timed hold.")
        }

        DescribedField {
            label:       qsTr("Settle time")
            fact:        _starMissionSettings.anchorSettle
            description: qsTr("How long it has to stay inside that radius without once leaving. Drifting out resets this timer to zero and it starts over.")
        }

        DescribedField {
            label:       qsTr("Guard timeout (0 = auto)")
            fact:        _starMissionSettings.anchorGuard
            description: qsTr("Hard limit on a single anchor, counted from the moment it starts. When it runs out the mission moves on whether or not the vehicle ever settled or finished turning, so a bad anchor cannot stall the whole plan. This matters more now that the turn and the shutter happen inside the anchor. 0 lets the firmware work one out from the duration, the pre-shutter wait and whether there is a turn.")
        }
    }

    SettingsGroupLayout {
        Layout.fillWidth:   true
        heading:            qsTr("Photo Timing")
        headingDescription: qsTr("The shutter sequence at each photo target: turn to the camera heading, wait, fire, hold. With Anchor on the anchor command runs this sequence itself; without it the same steps are separate mission items.")

        DescribedField {
            label:       qsTr("Delay before shutter")
            fact:        _starMissionSettings.photoBefore
            description: qsTr("Pause between finishing the camera turn and firing the shutter, so the vehicle has stopped swinging when the photo is taken. Capped at 31 s inside an anchor.")
        }

        DescribedField {
            label:       qsTr("Photo window / anchor duration")
            fact:        _starMissionSettings.photoWindow
            description: qsTr("How long the vehicle stays on the point. With Anchor on this is the plain hold after the shutter -- the anchor already sequences the turn and the shutter, so nothing has to be padded to fit. Without Anchor it is the whole window and has to outlast the turn plus the delay above, otherwise the firmware drops the pending queue and the shutter never fires.")
        }
    }

    SettingsGroupLayout {
        Layout.fillWidth:   true
        heading:            qsTr("Start Gate")
        headingDescription: qsTr("What the plan does before the first dive. The vehicle is driven onto the start marker by hand, so the mission opens by holding that point and then telling the EKF where it is: MAV_CMD_AURA_POSITION_FIX (31015) snaps the navigation solution onto the plan's start coordinate, the same operation as SonarView's Set Location.")

        DescribedField {
            label:       qsTr("Start hold")
            fact:        _starMissionSettings.startWait
            description: qsTr("How long the vehicle holds the start point before the solution is snapped onto it. This is the window in which it stops moving and the estimator settles -- snapping while it is still gliding writes that glide straight back in. 0 removes the hold and the position fix together. The fix is only accepted while dead reckoning: with UGPS, GPS_INPUT or SITL's fake GPS still fusing the firmware rejects it, says so, and carries on.")
        }
    }

    SettingsGroupLayout {
        Layout.fillWidth:   true
        heading:            qsTr("Dive / Surface Holds")
        headingDescription: qsTr("Extra wait on the vertical legs of the pattern.")

        DescribedField {
            label:       qsTr("Dive settle")
            fact:        _starMissionSettings.diveSettle
            description: qsTr("Wait on the dive-in-place waypoint before travelling. ArduSub calls a waypoint reached once it is within WPNAV_RADIUS in 3D, so a short dive completes while the vehicle is still shallow and the travel leg then runs at the wrong depth. This hold keeps it there until it is actually down.")
        }

        DescribedField {
            label:       qsTr("Surface settle")
            fact:        _starMissionSettings.surfaceSettle
            description: qsTr("Wait after surfacing, before the camera turn and shutter sequence starts.")
        }
    }

    SettingsGroupLayout {
        Layout.fillWidth:   true
        showDividers:       false

        QGCButton {
            Layout.alignment:   Qt.AlignHCenter
            text:               qsTr("Reset to defaults")
            onClicked:          mainWindow.showMessageDialog(
                                    qsTr("Reset to defaults"),
                                    qsTr("Restore every value on this page to its default?\n\n") +
                                        qsTr("Settle radius 1.0 m, settle time 3 s, guard timeout auto, ") +
                                        qsTr("delay before shutter 3 s, photo window 5 s, ") +
                                        qsTr("start hold 10 s, dive settle 5 s, surface settle 1 s."),
                                    Dialog.Ok | Dialog.Cancel,
                                    function () {
                                        _starMissionSettings.resetToDefaults()
                                    })
        }
    }
}
