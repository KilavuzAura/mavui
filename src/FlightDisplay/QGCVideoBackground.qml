/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Controls

import QGroundControl

// Picks the video item matching the compiled-in backend. VideoManager locates
// the video surface via findChild on its objectName and reinterpret_casts it to
// the backend's item type, so the objectName must live on the loaded item —
// not on this wrapper — or the cast lands on the wrong object and crashes.
Item {
    id: videoBackground

    Loader {
        anchors.fill: parent
        source: QGroundControl.videoManager.gstreamerEnabled ? "FlightDisplayViewGStreamer.qml"
              : QGroundControl.videoManager.qtmultimediaEnabled ? "FlightDisplayViewQtMultimedia.qml"
              : "FlightDisplayViewDummy.qml"
        onLoaded: {
            item.objectName = videoBackground.objectName
            videoBackground.objectName = ""
        }
    }
}
