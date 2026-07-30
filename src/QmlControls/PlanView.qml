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
import QtQuick.Dialogs
import QtLocation
import QtPositioning
import QtQuick.Layouts
import QtQuick.Window

import QGroundControl
import QGroundControl.FlightMap
import QGroundControl.ScreenTools
import QGroundControl.Controls
import QGroundControl.FactSystem
import QGroundControl.FactControls
import QGroundControl.Palette
import QGroundControl.Controllers
import QGroundControl.ShapeFileHelper
import QGroundControl.FlightDisplay
import QGroundControl.UTMSP


Item {
    id: _root

    property bool planControlColapsed: false

    readonly property int   _decimalPlaces:             8
    readonly property real  _margin:                    ScreenTools.defaultFontPixelHeight * 0.5
    readonly property real  _toolsMargin:               ScreenTools.defaultFontPixelWidth * 0.75
    readonly property real  _radius:                    ScreenTools.defaultFontPixelWidth  * 0.5
    readonly property real  _rightPanelWidth:           Math.min(width / 3, ScreenTools.defaultFontPixelWidth * 30)
    readonly property var   _defaultVehicleCoordinate:  QtPositioning.coordinate(37.803784, -122.462276)
    readonly property bool  _waypointsOnlyMode:         QGroundControl.corePlugin.options.missionWaypointsOnly

    property var    _planMasterController:              planMasterController
    property var    _missionController:                 _planMasterController.missionController
    property var    _geoFenceController:                _planMasterController.geoFenceController
    property var    _rallyPointController:              _planMasterController.rallyPointController
    property var    _visualItems:                       _missionController.visualItems
    property bool   _lightWidgetBorders:                editorMap.isSatelliteMap
    property bool   _addROIOnClick:                     false
    property bool   _singleComplexItem:                 _missionController.complexMissionItemNames.length === 1
    property bool   _starMissionMode:                   false   ///< Star Mission One placement mode active
    property bool   _starMissionHomeSet:                false   ///< first map click in the mode sets home, rest add targets
    property bool   _starMissionSetStart:               false   ///< armed by the banner: next map click re-places the start point
    property var    _starMissionSeed:                   []      ///< per-target yaw/anchor recovered from the plan the mode was opened on
    property var    _starMissionCreator:                null    ///< the StarMissionOne plan creator (for the table entry path)
     property int    _editingLayer:                      {if(!_utmspEnabled){layerTabBar.currentIndex ? _layers[layerTabBar.currentIndex] : _layerMission}else{layerTabBarUTMSP.currentIndex ? _layersUTMSP[layerTabBarUTMSP.currentIndex] : _layerMission}}
    property int    _toolStripBottom:                   toolStrip.height + toolStrip.y
    property var    _appSettings:                       QGroundControl.settingsManager.appSettings
    property var    _planViewSettings:                  QGroundControl.settingsManager.planViewSettings
    property var    _starMissionSettings:               QGroundControl.settingsManager.starMissionSettings
    property bool   _promptForPlanUsageShowing:         false
    property bool   _utmspEnabled:                      QGroundControl.utmspSupported
    property bool   _resetGeofencePolygon:              false   //Reset the Geofence Polygon
    property var    _vehicleID
    property bool   _triggerSubmit
    property bool   _resetRegisterFlightPlan

    readonly property var       _layers:                    [_layerMission, _layerGeoFence, _layerRallyPoints]
    readonly property var       _layersUTMSP:               [_layerMission, _layerRallyPoints, _layerUTMSP] //Adds additional UTMSP layer

    readonly property int       _layerMission:              1
    readonly property int       _layerGeoFence:             2
    readonly property int       _layerRallyPoints:          3
    readonly property int       _layerUTMSP:                4 // Additional Tab button when UTMSP is enabled
    readonly property string    _armedVehicleUploadPrompt:  qsTr("Vehicle is currently armed. Do you want to upload the mission to the vehicle?")


    function mapCenter() {
        var coordinate = editorMap.center
        coordinate.latitude  = coordinate.latitude.toFixed(_decimalPlaces)
        coordinate.longitude = coordinate.longitude.toFixed(_decimalPlaces)
        coordinate.altitude  = coordinate.altitude.toFixed(_decimalPlaces)
        return coordinate
    }

    property bool _firstMissionLoadComplete:    false
    property bool _firstFenceLoadComplete:      false
    property bool _firstRallyLoadComplete:      false
    property bool _firstLoadComplete:           false

    MapFitFunctions {
        id:                         mapFitFunctions  // The name for this id cannot be changed without breaking references outside of this code. Beware!
        map:                        editorMap
        usePlannedHomePosition:     true
        planMasterController:       _planMasterController
    }

    onVisibleChanged: {
        if(visible) {
            editorMap.zoomLevel = QGroundControl.flightMapZoom
            editorMap.center    = QGroundControl.flightMapPosition
            if (!_planMasterController.containsItems) {
                toolStrip.simulateClick(toolStrip.fileButtonIndex)
            }
        }
    }

    Connections {
        target: _appSettings ? _appSettings.defaultMissionItemAltitude : null
        function onRawValueChanged() {
            if (_visualItems.count > 1) {
                mainWindow.showMessageDialog(qsTr("Apply new altitude"),
                                             qsTr("You have changed the default altitude for mission items. Would you like to apply that altitude to all the items in the current mission?"),
                                             Dialog.Yes | Dialog.No,
                                             function() { _missionController.applyDefaultMissionAltitude() })
            }
        }
    }

    Component {
        id: promptForPlanUsageOnVehicleChangePopupComponent
        QGCPopupDialog {
            title:      _planMasterController.managerVehicle.isOfflineEditingVehicle ? qsTr("Plan View - Vehicle Disconnected") : qsTr("Plan View - Vehicle Changed")
            buttons:    Dialog.NoButton

            ColumnLayout {
                QGCLabel {
                    Layout.maximumWidth:    parent.width
                    wrapMode:               QGCLabel.WordWrap
                    text:                   _planMasterController.managerVehicle.isOfflineEditingVehicle ?
                                                qsTr("The vehicle associated with the plan in the Plan View is no longer available. What would you like to do with that plan?") :
                                                qsTr("The plan being worked on in the Plan View is not from the current vehicle. What would you like to do with that plan?")
                }

                QGCButton {
                    Layout.fillWidth:   true
                    text:               _planMasterController.dirty ?
                                            (_planMasterController.managerVehicle.isOfflineEditingVehicle ?
                                                 qsTr("Discard Unsaved Changes") :
                                                 qsTr("Discard Unsaved Changes, Load New Plan From Vehicle")) :
                                            qsTr("Load New Plan From Vehicle")
                    onClicked: {
                        _planMasterController.showPlanFromManagerVehicle()
                        _promptForPlanUsageShowing = false
                        close();
                    }
                }

                QGCButton {
                    Layout.fillWidth:   true
                    text:               _planMasterController.managerVehicle.isOfflineEditingVehicle ?
                                            qsTr("Keep Current Plan") :
                                            qsTr("Keep Current Plan, Don't Update From Vehicle")
                    onClicked: {
                        if (!_planMasterController.managerVehicle.isOfflineEditingVehicle) {
                            _planMasterController.dirty = true
                        }
                        _promptForPlanUsageShowing = false
                        close()
                    }
                }
            }
        }
    }

    PlanMasterController {
        id:         planMasterController
        flyView:    false

        Component.onCompleted: {
            _planMasterController.start()
            _missionController.setCurrentPlanViewSeqNum(0, true)
        }

        onPromptForPlanUsageOnVehicleChange: {
            if (!_promptForPlanUsageShowing) {
                _promptForPlanUsageShowing = true
                promptForPlanUsageOnVehicleChangePopupComponent.createObject(mainWindow).open()
            }
        }

        function waitingOnIncompleteDataMessage(save) {
            var saveOrUpload = save ? qsTr("Save") : qsTr("Upload")
            mainWindow.showMessageDialog(qsTr("Unable to %1").arg(saveOrUpload), qsTr("Plan has incomplete items. Complete all items and %1 again.").arg(saveOrUpload))
        }

        function waitingOnTerrainDataMessage(save) {
            var saveOrUpload = save ? qsTr("Save") : qsTr("Upload")
            mainWindow.showMessageDialog(qsTr("Unable to %1").arg(saveOrUpload), qsTr("Plan is waiting on terrain data from server for correct altitude values."))
        }

        function checkReadyForSaveUpload(save) {
            if (readyForSaveState() == VisualMissionItem.NotReadyForSaveData) {
                waitingOnIncompleteDataMessage(save)
                return false
            } else if (readyForSaveState() == VisualMissionItem.NotReadyForSaveTerrain) {
                waitingOnTerrainDataMessage(save)
                return false
            }
            return true
        }

        function upload() {
            if (!checkReadyForSaveUpload(false /* save */)) {
                return
            }
            switch (_missionController.sendToVehiclePreCheck()) {
                case MissionController.SendToVehiclePreCheckStateOk:
                    sendToVehicle()
                    break
                case MissionController.SendToVehiclePreCheckStateActiveMission:
                    mainWindow.showMessageDialog(qsTr("Send To Vehicle"), qsTr("Current mission must be paused prior to uploading a new Plan"))
                    break
                case MissionController.SendToVehiclePreCheckStateFirwmareVehicleMismatch:
                    mainWindow.showMessageDialog(qsTr("Plan Upload"),
                                                 qsTr("This Plan was created for a different firmware or vehicle type than the firmware/vehicle type of vehicle you are uploading to. " +
                                                      "This can lead to errors or incorrect behavior. " +
                                                      "It is recommended to recreate the Plan for the correct firmware/vehicle type.\n\n" +
                                                      "Click 'Ok' to upload the Plan anyway."),
                                                 Dialog.Ok | Dialog.Cancel,
                                                 function() { _planMasterController.sendToVehicle() })
                    break
            }
        }

        function loadFromSelectedFile() {
            fileDialog.title =          qsTr("Select Plan File")
            fileDialog.planFiles =      true
            fileDialog.nameFilters =    _planMasterController.loadNameFilters
            fileDialog.openForLoad()
        }

        function saveToSelectedFile() {
            if (!checkReadyForSaveUpload(true /* save */)) {
                return
            }
            fileDialog.title =          qsTr("Save Plan")
            fileDialog.planFiles =      true
            fileDialog.nameFilters =    _planMasterController.saveNameFilters
            fileDialog.openForSave()
        }

        function fitViewportToItems() {
            mapFitFunctions.fitMapViewportToMissionItems()
        }

        function saveKmlToSelectedFile() {
            if (!checkReadyForSaveUpload(true /* save */)) {
                return
            }
            fileDialog.title =          qsTr("Save KML")
            fileDialog.planFiles =      false
            fileDialog.nameFilters =    ShapeFileHelper.fileDialogKMLFilters
            fileDialog.openForSave()
        }
    }

    Connections {
        target: _missionController

        function onNewItemsFromVehicle() {
            if (_visualItems && _visualItems.count !== 1) {
                mapFitFunctions.fitMapViewportToMissionItems()
            }
            _missionController.setCurrentPlanViewSeqNum(0, true)
        }
    }

    function insertSimpleItemAfterCurrent(coordinate) {
        var nextIndex = _missionController.currentPlanViewVIIndex + 1
        _missionController.insertSimpleMissionItem(coordinate, nextIndex, true /* makeCurrentItem */)
    }

    // ---- Star Mission One placement mode ----
    function _startStarMissionMode(creator) {
        _starMissionCreator = creator
        // Read an already-loaded plan back into its targets BEFORE removeAll() destroys
        // it, then put them back on the map. From there the mode behaves exactly as if
        // the user had just clicked them, so the map, the Table, Undo and Finish all
        // work unchanged - and reopening the mode on a plan no longer means retyping it.
        var recovered = _planMasterController.containsItems ? creator.extractTargets() : []
        var start     = _missionController.plannedHomePosition
        _planMasterController.removeAll()
        _starMissionHomeSet  = false
        _starMissionSetStart = false
        _starMissionSeed     = recovered
        _starMissionMode     = true
        // Arm the Waypoint tool so entering the mode still means "click to place",
        // but the user can now switch it off to pan or select without placing.
        addWaypointRallyPointAction.checked = true
        _starMissionRestore(start, recovered)
    }

    // Puts targets recovered from an existing plan back on the map as ordinary placed
    // points. Their heading and anchor flag cannot live on a map item, so they stay in
    // _starMissionSeed and are re-attached by index in _starMissionPlacedTargets().
    function _starMissionRestore(start, targets) {
        if (targets.length === 0) {
            return
        }
        var settingsItem = _visualItems.get(0)
        if (settingsItem && start && start.isValid) {
            settingsItem.coordinate = start
            _starMissionHomeSet = true
        }
        for (var i = 0; i < targets.length; i++) {
            var item = _missionController.insertSimpleMissionItem(targets[i].coordinate, -1, false /* makeCurrentItem */)
            if (item && item.cameraSection) {
                item.cameraSection.cameraAction.rawValue = targets[i].camera ? 6 : 0   // TakePhoto : None
            }
        }
        // Leave the start point selected, so it is the one thing already draggable.
        _missionController.setCurrentPlanViewSeqNum(0, true)
    }

    function _exitStarMissionMode() {
        _starMissionMode = false
        _starMissionHomeSet = false
        _starMissionSetStart = false
        _starMissionSeed = []
        addWaypointRallyPointAction.checked = false
    }

    // Start point currently placed on the map, or null if nothing has been placed.
    function _starMissionStartCoord() {
        var settingsItem = _visualItems.get(0)
        if (!_starMissionHomeSet || !settingsItem || !settingsItem.coordinate.isValid) {
            return null
        }
        return settingsItem.coordinate
    }

    // Targets currently placed on the map, in order. Lets the Table open on what is
    // already there instead of a blank sheet: click roughly where the targets go,
    // then open the table to type exact coordinates and per-target headings. Without
    // this the table seeded Home from the map centre and threw the placement away.
    function _starMissionPlacedTargets() {
        var targets = []
        for (var i = 1; i < _visualItems.count; i++) {
            var it = _visualItems.get(i)
            if (!it.specifiesCoordinate) {
                continue
            }
            // A map item cannot carry a camera heading or an anchor flag. For targets
            // recovered from an existing plan those come back from _starMissionSeed by
            // index; anything clicked afterwards falls back to no heading and the
            // banner's Anchor box.
            var seed = targets.length < _starMissionSeed.length ? _starMissionSeed[targets.length] : null
            targets.push({ coordinate: it.coordinate,
                           camera:     it.cameraSection ? (it.cameraSection.cameraAction.rawValue === 6) : false,
                           yaw:        seed ? seed.yaw : -1,
                           anchor:     seed ? seed.anchor : starMissionAnchorCheck.checked })
        }
        return targets
    }

    // Drops the last placed target. A stray map click used to be unrecoverable:
    // every click adds a target and Finish sweeps every item after index 0 into
    // the plan, so the only way out was Cancel, which wipes the whole placement.
    function _starMissionUndo() {
        if (_visualItems.count > 1) {
            _missionController.removeVisualItem(_visualItems.count - 1)
        }
    }

    function _cancelStarMissionMode() {
        _planMasterController.removeAll()
        _exitStarMissionMode()
    }

    // Handles a map click while in Star Mission One mode: the first click places the
    // start point, every following click adds a target waypoint (camera on by default).
    // The banner's "Move start" button re-arms the start branch, because _starMissionHomeSet
    // used to be a one-way latch: a misplaced first click could only be undone by
    // Cancel, which throws away every target placed since.
    function _starMissionMapClick(coordinate) {
        if (!_starMissionHomeSet || _starMissionSetStart) {
            var settingsItem = _visualItems.get(0)   // index 0 is always the mission settings (start) item
            if (settingsItem) {
                settingsItem.coordinate = coordinate
            }
            _starMissionHomeSet  = true
            _starMissionSetStart = false
            // Select it, so its drag handle appears: SimpleItemMapVisual only shows the
            // drag area for the current item, and this flow never made index 0 current -
            // the start point could be placed but then never nudged on the map.
            _missionController.setCurrentPlanViewSeqNum(0, true)
        } else {
            var item = _missionController.insertSimpleMissionItem(coordinate, -1, true /* makeCurrentItem */)
            if (item && item.cameraSection) {
                item.cameraSection.cameraAction.rawValue = 6 // CameraSection::TakePhoto — camera on by default
            }
        }
    }

    // Cruise depth from a UI text box: empty/invalid falls back to -1 m and a
    // positive number is read as a depth (the vehicle must never be sent above
    // the surface). Kept in one place so the banner and the table agree.
    function _starMissionDepth(depthText) {
        var depth = parseFloat(depthText)
        if (isNaN(depth) || depth === 0) {
            return -1.0
        }
        return depth > 0 ? -depth : depth
    }

    // Surface/bottom clearance box: a distance, so the sign is dropped. Empty or
    // nonsense returns NaN and the plan creator falls back to its own default.
    function _starMissionClearance(text) {
        var value = parseFloat(text)
        return (isNaN(value) || value === 0) ? NaN : Math.abs(value)
    }

    // Photo timing box (seconds). Empty or nonsense returns NaN so the plan creator
    // keeps its own default (3 s before the shutter, 5 s window).
    function _starMissionWait(text) {
        var value = parseFloat(text)
        return (isNaN(value) || value < 0) ? NaN : value
    }

    // Reads the placed home + target waypoints and expands them into the full
    // dive/surface/photo pattern via the plan creator.
    function _finishStarMissionMode() {
        if (!_starMissionCreator || !_starMissionHomeSet) {
            _exitStarMissionMode()
            return
        }
        var home = _visualItems.get(0).coordinate
        // Same list the Table opens on. Click-to-place has no per-target checkbox, so
        // the banner "Anchor" applies to every freshly clicked target; targets carried
        // over from an existing plan keep their own heading and anchor flag.
        var targets = _starMissionPlacedTargets()
        var depth       = _starMissionDepth(starMissionDepthField.text)
        var autoDepth   = starMissionAutoDepthCheck.checked
        var surface     = _starMissionClearance(starMissionSurfaceField.text)
        var bottom      = _starMissionClearance(starMissionBottomField.text)
        var photoBefore = _starMissionWait(starMissionPhotoBeforeField.text)
        var photoWindow = _starMissionWait(starMissionPhotoWindowField.text)
        var startAnchor = starMissionStartAnchorCheck.checked
        _exitStarMissionMode()
        if (targets.length > 0) {
            _starMissionCreator.createFullPlan(home, targets, depth, autoDepth, surface, bottom,
                                               photoBefore, photoWindow, startAnchor)
        }
    }

    function insertROIAfterCurrent(coordinate) {
        var nextIndex = _missionController.currentPlanViewVIIndex + 1
        _missionController.insertROIMissionItem(coordinate, nextIndex, true /* makeCurrentItem */)
    }

    function insertCancelROIAfterCurrent() {
        var nextIndex = _missionController.currentPlanViewVIIndex + 1
        _missionController.insertCancelROIMissionItem(nextIndex, true /* makeCurrentItem */)
    }

    function insertComplexItemAfterCurrent(complexItemName) {
        var nextIndex = _missionController.currentPlanViewVIIndex + 1
        _missionController.insertComplexMissionItem(complexItemName, mapCenter(), nextIndex, true /* makeCurrentItem */)
    }

    function insertTakeoffItemAfterCurrent() {
        var nextIndex = _missionController.currentPlanViewVIIndex + 1
        _missionController.insertTakeoffItem(mapCenter(), nextIndex, true /* makeCurrentItem */)
    }

    function insertLandItemAfterCurrent() {
        var nextIndex = _missionController.currentPlanViewVIIndex + 1
        _missionController.insertLandItem(mapCenter(), nextIndex, true /* makeCurrentItem */)
    }


    function selectNextNotReady() {
        var foundCurrent = false
        for (var i=0; i<_missionController.visualItems.count; i++) {
            var vmi = _missionController.visualItems.get(i)
            if (vmi.readyForSaveState === VisualMissionItem.NotReadyForSaveData) {
                _missionController.setCurrentPlanViewSeqNum(vmi.sequenceNumber, true)
                break
            }
        }
    }

    QGCFileDialog {
        id:             fileDialog
        folder:         _appSettings ? _appSettings.missionSavePath : ""

        property bool planFiles: true    ///< true: working with plan files, false: working with kml file

        onAcceptedForSave: (file) => {
            if (planFiles) {
                _planMasterController.saveToFile(file)
            } else {
                _planMasterController.saveToKml(file)
            }
            close()
        }

        onAcceptedForLoad: (file) => {
            _planMasterController.loadFromFile(file)
            _planMasterController.fitViewportToItems()
            _missionController.setCurrentPlanViewSeqNum(0, true)
            close()
        }
    }

    PlanViewToolBar {
        id:                     planToolBar
        planMasterController:   _planMasterController
    }

    Item {
        id:             panel
        anchors.left:   parent.left
        anchors.right:  parent.right
        anchors.top:    planToolBar.bottom
        anchors.bottom: parent.bottom

        FlightMap {
            id:                         editorMap
            anchors.fill:               parent
            mapName:                    "MissionEditor"
            allowGCSLocationCenter:     true
            allowVehicleLocationCenter: true
            planView:                   true

            zoomLevel:                  QGroundControl.flightMapZoom
            center:                     QGroundControl.flightMapPosition

            // This is the center rectangle of the map which is not obscured by tools
            property rect centerViewport:   Qt.rect(_leftToolWidth + _margin,  _margin, editorMap.width - _leftToolWidth - _rightToolWidth - (_margin * 2), (terrainStatus.visible ? terrainStatus.y : height - _margin) - _margin)

            property real _leftToolWidth:       toolStrip.x + toolStrip.width
            property real _rightToolWidth:      rightPanel.width + rightPanel.anchors.rightMargin
            property real _nonInteractiveOpacity:  0.5

            // Initial map position duplicates Fly view position
            Component.onCompleted: editorMap.center = QGroundControl.flightMapPosition

            QGCMapPalette { id: mapPal; lightColors: editorMap.isSatelliteMap }

            onZoomLevelChanged: {
                QGroundControl.flightMapZoom = editorMap.zoomLevel
            }
            onCenterChanged: {
                QGroundControl.flightMapPosition = editorMap.center
            }

            onMapClicked: (mouse) => {
                // Take focus to close any previous editing
                editorMap.focus = true
                if (!mainWindow.allowViewSwitch()) {
                    return
                }
                var coordinate = editorMap.toCoordinate(Qt.point(mouse.x, mouse.y), false /* clipToViewPort */)
                coordinate.latitude = coordinate.latitude.toFixed(_decimalPlaces)
                coordinate.longitude = coordinate.longitude.toFixed(_decimalPlaces)
                coordinate.altitude = coordinate.altitude.toFixed(_decimalPlaces)
				if(_utmspEnabled){
                	QGroundControl.utmspManager.utmspVehicle.updateLastCoordinates(coordinate.latitude, coordinate.longitude)
                }

                if (_starMissionMode) {
                    // Gate on the Waypoint tool exactly like the normal plan flow below.
                    // The mode used to swallow every map click unconditionally, so panning,
                    // dismissing a popup or trying to select the start marker all dropped a
                    // target wherever you happened to touch - and Finish sweeps every item
                    // after index 0 into the plan, so those strays became real survey stops.
                    // "Move start" stays exempt: it is an explicit one-shot from the banner.
                    if (addWaypointRallyPointAction.checked || _starMissionSetStart) {
                        _starMissionMapClick(coordinate)
                    }
                    return
                }

                switch (_editingLayer) {
                case _layerMission:
                    if (addWaypointRallyPointAction.checked) {
                        insertSimpleItemAfterCurrent(coordinate)
                    } else if (_addROIOnClick) {
                        insertROIAfterCurrent(coordinate)
                        _addROIOnClick = false
                    }

                    break
                case _layerRallyPoints:
                    if (_rallyPointController.supported && addWaypointRallyPointAction.checked) {
                        _rallyPointController.addPoint(coordinate)
                    }
                    break

                case _layerUTMSP:
                    if (addWaypointRallyPointAction.checked) {
                    	insertSimpleItemAfterCurrent(coordinate)
                    } else if (_addROIOnClick) {
                    	insertROIAfterCurrent(coordinate)
                        _addROIOnClick = false
                    }
                    break
                }
            }

            // Add the mission item visuals to the map
            Repeater {
                model: _missionController.visualItems
                delegate: MissionItemMapVisual {
                    map:         editorMap
                    opacity:     _editingLayer == _layerMission || _editingLayer == _layerUTMSP ? 1 : editorMap._nonInteractiveOpacity
                    interactive: _editingLayer == _layerMission || _editingLayer == _layerUTMSP
                    vehicle:     _planMasterController.controllerVehicle
                    onClicked:   (sequenceNumber) => { _missionController.setCurrentPlanViewSeqNum(sequenceNumber, false) }
                }
            }

            // Add lines between waypoints
            MissionLineView {
                showSpecialVisual:  _missionController.isROIBeginCurrentItem
                model:              _missionController.simpleFlightPathSegments
                opacity:            _editingLayer == _layerMission ||  _editingLayer == _layerUTMSP  ? 1 : editorMap._nonInteractiveOpacity
            }

            // Direction arrows in waypoint lines
            MapItemView {
                model: _editingLayer == _layerMission ||_editingLayer == _layerUTMSP ? _missionController.directionArrows : undefined

                delegate: MapLineArrow {
                    fromCoord:      object ? object.coordinate1 : undefined
                    toCoord:        object ? object.coordinate2 : undefined
                    arrowPosition:  3
                    z:              QGroundControl.zOrderWaypointLines + 1
                }
            }

            // Incomplete segment lines
            MapItemView {
                model: _missionController.incompleteComplexItemLines

                delegate: MapPolyline {
                    path:       [ object.coordinate1, object.coordinate2 ]
                    line.width: 1
                    line.color: "red"
                    z:          QGroundControl.zOrderWaypointLines
                    opacity:    _editingLayer == _layerMission ? 1 : editorMap._nonInteractiveOpacity
                }
            }

            // UI for splitting the current segment
            MapQuickItem {
                id:             splitSegmentItem
                anchorPoint.x:  sourceItem.width / 2
                anchorPoint.y:  sourceItem.height / 2
                z:              QGroundControl.zOrderWaypointLines + 1
                visible:        _editingLayer == _layerMission ||  _editingLayer == _layerUTMSP

                sourceItem: SplitIndicator {
                    onClicked:  _missionController.insertSimpleMissionItem(splitSegmentItem.coordinate,
                                                                           _missionController.currentPlanViewVIIndex,
                                                                           true /* makeCurrentItem */)
                }

                function _updateSplitCoord() {
                    if (_missionController.splitSegment) {
                        var distance = _missionController.splitSegment.coordinate1.distanceTo(_missionController.splitSegment.coordinate2)
                        var azimuth = _missionController.splitSegment.coordinate1.azimuthTo(_missionController.splitSegment.coordinate2)
                        splitSegmentItem.coordinate = _missionController.splitSegment.coordinate1.atDistanceAndAzimuth(distance / 2, azimuth)
                    } else {
                        coordinate = QtPositioning.coordinate()
                    }
                }

                Connections {
                    target:                 _missionController
                    function onSplitSegmentChanged()  { splitSegmentItem._updateSplitCoord() }
                }

                Connections {
                    target:                 _missionController.splitSegment
                    function onCoordinate1Changed()   { splitSegmentItem._updateSplitCoord() }
                    function onCoordinate2Changed()   { splitSegmentItem._updateSplitCoord() }
                }
            }

            // Add the vehicles to the map
            MapItemView {
                model: QGroundControl.multiVehicleManager.vehicles
                delegate: VehicleMapItem {
                    vehicle:        object
                    coordinate:     object.coordinate
                    map:            editorMap
                    size:           ScreenTools.defaultFontPixelHeight * 3
                    z:              QGroundControl.zOrderMapItems - 1
                }
            }

            GeoFenceMapVisuals {
                map:                    editorMap
                myGeoFenceController:   _geoFenceController
                interactive:            _editingLayer == _layerGeoFence
                homePosition:           _missionController.plannedHomePosition
                planView:               true
                opacity:                _editingLayer != _layerGeoFence ? editorMap._nonInteractiveOpacity : 1
            }

            RallyPointMapVisuals {
                map:                    editorMap
                myRallyPointController: _rallyPointController
                interactive:            _editingLayer == _layerRallyPoints
                planView:               true
                opacity:                _editingLayer != _layerRallyPoints ? editorMap._nonInteractiveOpacity : 1
            }

            UTMSPMapVisuals {
                id: utmspvisual
                enabled:                _utmspEnabled
                map:                    editorMap
                currentMissionItems:    _visualItems
                myGeoFenceController:   _geoFenceController
                interactive:            _editingLayer == _layerUTMSP
                homePosition:           _missionController.plannedHomePosition
                planView:               true
                opacity:                _editingLayer != _layerUTMSP ? editorMap._nonInteractiveOpacity : 1
                resetCheck:             _resetGeofencePolygon
            }

            Connections {
                target: utmspEditor
                function onResetGeofencePolygonTriggered() {
                    resetTimer.start()
                }
            }
            Timer {
                id: resetTimer
                interval: 2500
                running: false
                repeat: false
                onTriggered: {
                    _resetGeofencePolygon = true
                }
            }
        }

        //-----------------------------------------------------------
        // Left tool strip
        ToolStrip {
            id:                 toolStrip
            anchors.margins:    _toolsMargin
            anchors.left:       parent.left
            anchors.top:        parent.top
            z:                  QGroundControl.zOrderWidgets
            maxHeight:          parent.height - toolStrip.y

            readonly property int fileButtonIndex:      0
            readonly property int takeoffButtonIndex:   1
            readonly property int waypointButtonIndex:  2
            readonly property int roiButtonIndex:       3
            readonly property int patternButtonIndex:   4
            readonly property int landButtonIndex:      5
            readonly property int centerButtonIndex:    6

            property bool _isRallyLayer:    _editingLayer == _layerRallyPoints
            property bool _isMissionLayer:  _editingLayer == _layerMission
            property bool _isUtmspLayer:     _editingLayer == _layerUTMSP

            ToolStripActionList {
                id: toolStripActionList
                model: [
                    ToolStripAction {
                        text:                   qsTr("File")
                        enabled:                !_planMasterController.syncInProgress
                        visible:                true
                        showAlternateIcon:      _planMasterController.dirty
                        iconSource:             "/qmlimages/MapSync.svg"
                        alternateIconSource:    "/qmlimages/MapSyncChanged.svg"
                        dropPanelComponent:     syncDropPanel
                    },
                    ToolStripAction {
                        text:       qsTr("Takeoff")
                        iconSource: "/res/takeoff.svg"
                        enabled:    _missionController.isInsertTakeoffValid
                        visible:    (toolStrip._isMissionLayer || toolStrip._isUtmspLayer) && !_planMasterController.controllerVehicle.rover
                        onTriggered: {
                            toolStrip.allAddClickBoolsOff()
                            insertTakeoffItemAfterCurrent()
                            _triggerSubmit = true
                        }
                    },
                    ToolStripAction {
                        id:                 addWaypointRallyPointAction
                        text:               _editingLayer == _layerRallyPoints ? qsTr("Rally Point") : qsTr("Waypoint")
                        iconSource:         "/qmlimages/MapAddMission.svg"
                        enabled:            toolStrip._isRallyLayer ? true : _missionController.flyThroughCommandsAllowed
                        visible:            toolStrip._isRallyLayer || toolStrip._isMissionLayer || toolStrip._isUtmspLayer
                        checkable:          true
                    },
                    ToolStripAction {
                        text:               _missionController.isROIActive ? qsTr("Cancel ROI") : qsTr("ROI")
                        iconSource:         "/qmlimages/MapAddMission.svg"
                        enabled:            !_missionController.onlyInsertTakeoffValid
                        visible:            toolStrip._isMissionLayer && _planMasterController.controllerVehicle.roiModeSupported
                        checkable:          !_missionController.isROIActive
                        onCheckedChanged:   _addROIOnClick = checked
                        onTriggered: {
                            if (_missionController.isROIActive) {
                                toolStrip.allAddClickBoolsOff()
                                insertCancelROIAfterCurrent()
                            }
                        }
                        property bool myAddROIOnClick: _addROIOnClick
                        onMyAddROIOnClickChanged: checked = _addROIOnClick
                    },
                    ToolStripAction {
                        text:               _singleComplexItem ? _missionController.complexMissionItemNames[0] : qsTr("Pattern")
                        iconSource:         "/qmlimages/MapDrawShape.svg"
                        enabled:            _missionController.flyThroughCommandsAllowed
                        visible:            toolStrip._isMissionLayer
                        dropPanelComponent: _singleComplexItem ? undefined : patternDropPanel
                        onTriggered: {
                            toolStrip.allAddClickBoolsOff()
                            if (_singleComplexItem) {
                                insertComplexItemAfterCurrent(_missionController.complexMissionItemNames[0])
                            }
                        }
                    },
                    ToolStripAction {
                        text:       _planMasterController.controllerVehicle.multiRotor
                                    ? qsTr("Return")
                                    : _missionController.isInsertLandValid && _missionController.hasLandItem
                                      ? qsTr("Alt Land")
                                      : qsTr("Land")
                        iconSource: "/res/rtl.svg"
                        enabled:    _missionController.isInsertLandValid
                        visible:    toolStrip._isMissionLayer || toolStrip._isUtmspLayer
                        onTriggered: {
                            toolStrip.allAddClickBoolsOff()
                            insertLandItemAfterCurrent()
                        }
                    },
                    ToolStripAction {
                        text:               qsTr("Center")
                        iconSource:         "/qmlimages/MapCenter.svg"
                        enabled:            true
                        visible:            true
                        dropPanelComponent: centerMapDropPanel
                    }
                ]
            }

            model: toolStripActionList.model

            function allAddClickBoolsOff() {
                _addROIOnClick =        false
                addWaypointRallyPointAction.checked = false
            }

            onDropped: allAddClickBoolsOff()
        }

        //-----------------------------------------------------------
        // Star Mission One mode banner (top of map). Only visible while placing.
        Rectangle {
            id:                         starMissionBanner
            anchors.top:                parent.top
            anchors.topMargin:          _toolsMargin
            anchors.horizontalCenter:   parent.horizontalCenter
            z:                          QGroundControl.zOrderWidgets
            visible:                    _starMissionMode
            width:                      starMissionBannerCol.width + (ScreenTools.defaultFontPixelWidth * 3)
            height:                     starMissionBannerCol.height + (ScreenTools.defaultFontPixelHeight)
            radius:                     ScreenTools.defaultFontPixelHeight / 2
            color:                      qgcPal.window
            border.color:               qgcPal.buttonHighlight
            border.width:               1

            // Title/hint on the first line, controls on the second: as one row the
            // banner grew wider than the map and spilled out over its sides. The
            // labels also wrap so a narrow window never pushes them off the edge.
            ColumnLayout {
                id:                 starMissionBannerCol
                anchors.centerIn:   parent
                spacing:            ScreenTools.defaultFontPixelHeight / 4

                property real _maxTextWidth: editorMap.width - (ScreenTools.defaultFontPixelWidth * 8)

                RowLayout {
                    Layout.alignment:   Qt.AlignHCenter
                    Layout.maximumWidth: starMissionBannerCol._maxTextWidth
                    spacing:            ScreenTools.defaultFontPixelWidth

                    QGCLabel {
                        text:       qsTr("AUV Stars 2026 Mission One")
                        font.bold:  true
                        wrapMode:   Text.WordWrap
                        Layout.maximumWidth: starMissionBannerCol._maxTextWidth / 2
                    }

                    QGCLabel {
                        text:               _starMissionSetStart
                                                ? qsTr("Click map to move the start point")
                                                : !addWaypointRallyPointAction.checked
                                                    ? qsTr("Waypoint tool off - map clicks place nothing")
                                                    : _starMissionHomeSet
                                                        ? qsTr("Click map to add targets")
                                                        : qsTr("Click map to set the start point")
                        color:              _starMissionSetStart ? qgcPal.warningText : qgcPal.colorGrey
                        font.pointSize:     ScreenTools.smallFontPointSize
                        wrapMode:           Text.WordWrap
                        Layout.maximumWidth: starMissionBannerCol._maxTextWidth / 2
                    }
                }

                // Depth row. Bottom following (terrain frame, rangefinder) sits to the
                // left of the depth box and greys it out, since it replaces it.
                RowLayout {
                    Layout.alignment:   Qt.AlignHCenter
                    spacing:            ScreenTools.defaultFontPixelWidth

                    QGCCheckBox {
                        id:     starMissionAutoDepthCheck
                        text:   qsTr("Auto depth")
                    }

                    QGCLabel {
                        text:       qsTr("Surface (m)")
                        enabled:    starMissionAutoDepthCheck.checked
                    }

                    QGCTextField {
                        id:                     starMissionSurfaceField
                        Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 7
                        placeholderText:        qsTr("0.3")
                        enabled:                starMissionAutoDepthCheck.checked
                        // Shallowest a cruise leg may be commanded to (m below surface).
                    }

                    QGCLabel {
                        text:       qsTr("Bottom (m)")
                        enabled:    starMissionAutoDepthCheck.checked
                    }

                    QGCTextField {
                        id:                     starMissionBottomField
                        Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 7
                        placeholderText:        qsTr("1.0")
                        enabled:                starMissionAutoDepthCheck.checked
                        // Clearance held above the sea floor while cruising (m).
                    }

                    QGCLabel {
                        text:       qsTr("Depth (m)")
                        enabled:    !starMissionAutoDepthCheck.checked
                    }

                    QGCTextField {
                        id:                     starMissionDepthField
                        Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 8
                        placeholderText:        qsTr("-1.0")
                        enabled:                !starMissionAutoDepthCheck.checked
                        // Empty box = -1 m (see _starMissionDepth); negative = below surface.
                    }
                }

                // Anchor + photo timing. With Anchor on, the vehicle drops anchor at
                // the photo point: it locks the position and does not advance until it
                // has stayed inside the settle radius continuously, and the anchor
                // command itself does the camera turn and the shutter (requires
                // aurapilot MAV_CMD_AURA_ANCHOR / 31010 support).
                RowLayout {
                    Layout.alignment:   Qt.AlignHCenter
                    spacing:            ScreenTools.defaultFontPixelWidth

                    QGCCheckBox {
                        id:      starMissionAnchorCheck
                        text:    qsTr("Anchor")
                        // On by default: without the anchor a stop is only held by
                        // AC_WPNav's latched arrival, so the vehicle can drift off the
                        // point while the hold timer runs, and the camera turn plus the
                        // shutter go back to being separate queue items that a short
                        // hold silently drops.
                        checked: true
                    }

                    QGCCheckBox {
                        id:      starMissionStartAnchorCheck
                        text:    qsTr("Start anchor")
                        // Anchors once at the start point, right after the dive in place
                        // and before the first travel leg, so the mission departs from a
                        // position the vehicle has actually held. No photo, no turn.
                        // On by default: the dive drags the vehicle off the start point,
                        // and without this gate that error is carried into every target.
                        checked: true
                    }

                    QGCLabel { text: qsTr("Foto öncesi (sn)") }

                    QGCTextField {
                        id:                     starMissionPhotoBeforeField
                        Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 7
                        placeholderText:        _starMissionSettings.photoBefore.valueString
                        // CONDITION_DELAY before the shutter. Empty = the App Settings ->
                        // Mission One value shown as the placeholder.
                    }

                    QGCLabel { text: qsTr("Foto penceresi (sn)") }

                    QGCTextField {
                        id:                     starMissionPhotoWindowField
                        Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 7
                        placeholderText:        _starMissionSettings.photoWindow.valueString
                        // With Anchor on: the plain hold AFTER the shutter (the anchor
                        // sequences the turn and the shutter itself). Without Anchor:
                        // the whole photo window, padded to outlast the queue.
                        // Empty = the App Settings -> Mission One value.
                    }
                }

                RowLayout {
                    Layout.alignment:   Qt.AlignHCenter
                    spacing:            ScreenTools.defaultFontPixelWidth

                    QGCButton {
                        text:       qsTr("Move start")
                        checkable:  true
                        checked:    _starMissionSetStart
                        enabled:    _starMissionHomeSet
                        onClicked:  _starMissionSetStart = checked
                    }

                    QGCButton {
                        text:       qsTr("Undo")
                        enabled:    _visualItems.count > 1
                        onClicked:  _starMissionUndo()
                    }

                    QGCButton {
                        text:       qsTr("Table")
                        onClicked:  starMissionOneDialog.createObject(mainWindow, { mapCenter: _mapCenter(),
                                                                                    startCoord:  _starMissionStartCoord(),
                                                                                    seedTargets: _starMissionPlacedTargets(),
                                                                                    planCreator: _starMissionCreator,
                                                                                    depthText:   starMissionDepthField.text,
                                                                                    autoDepth:   starMissionAutoDepthCheck.checked,
                                                                                    surfaceText: starMissionSurfaceField.text,
                                                                                    bottomText:  starMissionBottomField.text,
                                                                                    anchorAll:   starMissionAnchorCheck.checked,
                                                                                    startAnchor: starMissionStartAnchorCheck.checked,
                                                                                    photoBeforeText: starMissionPhotoBeforeField.text,
                                                                                    photoWindowText: starMissionPhotoWindowField.text }).open()

                        function _mapCenter() {
                            var centerPoint = Qt.point(editorMap.centerViewport.left + (editorMap.centerViewport.width / 2), editorMap.centerViewport.top + (editorMap.centerViewport.height / 2))
                            return editorMap.toCoordinate(centerPoint, false /* clipToViewPort */)
                        }
                    }

                    QGCButton {
                        text:       qsTr("Finish")
                        primary:    true
                        onClicked:  _finishStarMissionMode()
                    }

                    QGCButton {
                        text:       qsTr("Cancel")
                        onClicked:  _cancelStarMissionMode()
                    }
                }
            }
        }

        //-----------------------------------------------------------
        // Right pane for mission editing controls
        Rectangle {
            id:                 rightPanel
            height:             parent.height
            width:{
                 if(_utmspEnabled){
                     _rightPanelWidth + ScreenTools.defaultFontPixelWidth * 21.667
                 }
                 else{
                     _rightPanelWidth
                 }
             }
            color:              qgcPal.window
            opacity:            layerTabBar.visible ? 0.2 : 0
            anchors.bottom:     parent.bottom
            anchors.right:      parent.right
            anchors.rightMargin: _toolsMargin
        }
        //-------------------------------------------------------
        // Right Panel Controls
        Item {
            anchors.fill:           rightPanel
            anchors.topMargin:      _toolsMargin
            DeadMouseArea {
                anchors.fill:   parent
            }
            Column {
                id:                 rightControls
                spacing:            ScreenTools.defaultFontPixelHeight * 0.5
                anchors.left:       parent.left
                anchors.right:      parent.right
                anchors.top:        parent.top
                //-------------------------------------------------------
                // Mission Controls (Expanded)
                QGCTabBar {
                    id:         layerTabBar
                    width:      parent.width
                    visible:    QGroundControl.corePlugin.options.enablePlanViewSelector  && !_utmspEnabled
                    Component.onCompleted: currentIndex = 0
                    QGCTabButton {
                        text:       qsTr("Mission")
                    }
                    QGCTabButton {
                        text:       qsTr("Fence")
                        enabled:    _geoFenceController.supported
                    }
                    QGCTabButton {
                        text:       qsTr("Rally")
                        enabled:    _rallyPointController.supported
                    }
                }

                QGCTabBar {
                    id:         layerTabBarUTMSP
                    width:      parent.width
                    visible:    QGroundControl.corePlugin.options.enablePlanViewSelector && _utmspEnabled
                    QGCTabButton {
                        text:       qsTr("Mission")
                    }
                    QGCTabButton {
                        text:       qsTr("Rally")
                        enabled:    _rallyPointController.supported
                    }
                    QGCTabButton {
                        id: utmspbutton
                        text:       qsTr("UTM-Adapter")
                        visible: _utmspEnabled
                    }
                }
            }
            //-------------------------------------------------------
            // Mission Item Editor
            Item {
                id:                     missionItemEditor
                anchors.left:           parent.left
                anchors.right:          parent.right
                anchors.top:            rightControls.bottom
                anchors.topMargin:      ScreenTools.defaultFontPixelHeight * 0.25
                anchors.bottom:         parent.bottom
                anchors.bottomMargin:   ScreenTools.defaultFontPixelHeight * 0.25
                visible:                _editingLayer == _layerMission && !planControlColapsed
                QGCListView {
                    id:                 missionItemEditorListView
                    anchors.fill:       parent
                    spacing:            ScreenTools.defaultFontPixelHeight / 4
                    orientation:        ListView.Vertical
                    model:              _missionController.visualItems
                    cacheBuffer:        Math.max(height * 2, 0)
                    clip:               true
                    currentIndex:       _missionController.currentPlanViewSeqNum
                    highlightMoveDuration: 250
                    visible:            _editingLayer == _layerMission && !planControlColapsed
                    //-- List Elements
                    delegate: MissionItemEditor {
                        map:            editorMap
                        masterController:  _planMasterController
                        missionItem:    object
                        width:          missionItemEditorListView.width
                        readOnly:       false
                        onClicked: (sequenceNumber) => { _missionController.setCurrentPlanViewSeqNum(object.sequenceNumber, false) }
                        onRemove: {
                            var removeVIIndex = index
                            _missionController.removeVisualItem(removeVIIndex)
                            if (removeVIIndex >= _missionController.visualItems.count) {
                                removeVIIndex--
                            }
                        }
                        onSelectNextNotReadyItem:   selectNextNotReady()
                    }
                }
            }
            // GeoFence Editor
            GeoFenceEditor {
                anchors.top:            rightControls.bottom
                anchors.topMargin:      ScreenTools.defaultFontPixelHeight * 0.25
                anchors.bottom:         parent.bottom
                anchors.left:           parent.left
                anchors.right:          parent.right
                myGeoFenceController:   _geoFenceController
                flightMap:              editorMap
                visible:                _editingLayer == _layerGeoFence
            }

            // Rally Point Editor
            RallyPointEditorHeader {
                id:                     rallyPointHeader
                anchors.top:            rightControls.bottom
                anchors.topMargin:      ScreenTools.defaultFontPixelHeight * 0.25
                anchors.left:           parent.left
                anchors.right:          parent.right
                visible:                _editingLayer == _layerRallyPoints
                controller:             _rallyPointController
            }
            RallyPointItemEditor {
                id:                     rallyPointEditor
                anchors.top:            rallyPointHeader.bottom
                anchors.topMargin:      ScreenTools.defaultFontPixelHeight * 0.25
                anchors.left:           parent.left
                anchors.right:          parent.right
                visible:                _editingLayer == _layerRallyPoints && _rallyPointController.points.count
                rallyPoint:             _rallyPointController.currentRallyPoint
                controller:             _rallyPointController
            }
            UTMSPAdapterEditor{
                id: utmspEditor
                enabled:                 _utmspEnabled
                anchors.top:             rightControls.bottom
                anchors.topMargin:       ScreenTools.defaultFontPixelHeight * 0.25
                anchors.bottom:          parent.bottom
                anchors.left:            parent.left
                anchors.right:           parent.right
                currentMissionItems:     _visualItems
                myGeoFenceController:    _geoFenceController
                flightMap:               editorMap
                visible:                 _editingLayer == _layerUTMSP
                triggerSubmitButton:     _triggerSubmit
                resetRegisterFlightPlan: _resetRegisterFlightPlan
            }
        }

        QGCLabel {
            // Elevation provider notice on top of terrain plot
            readonly property string _licenseString: QGroundControl.elevationProviderNotice

            id:                         licenseLabel
            visible:                    terrainStatus.visible && _licenseString !== ""
            anchors.bottom:             terrainStatus.top
            anchors.horizontalCenter:   terrainStatus.horizontalCenter
            anchors.bottomMargin:       ScreenTools.defaultFontPixelWidth * 0.5
            font.pointSize:             ScreenTools.smallFontPointSize
            text:                       qsTr("Powered by %1").arg(_licenseString)
        }

        TerrainStatus {
            id:                 terrainStatus
            anchors.margins:    _toolsMargin
            anchors.leftMargin: 0
            anchors.left:       mapScale.left
            anchors.right:      rightPanel.left
            anchors.bottom:     parent.bottom
            height:             ScreenTools.defaultFontPixelHeight * 7
            missionController:  _missionController
            visible:            _internalVisible && _editingLayer === _layerMission && QGroundControl.corePlugin.options.showMissionStatus

            onSetCurrentSeqNum: _missionController.setCurrentPlanViewSeqNum(seqNum, true)

            property bool _internalVisible: _planViewSettings.showMissionItemStatus.rawValue

            function toggleVisible() {
                _internalVisible = !_internalVisible
                _planViewSettings.showMissionItemStatus.rawValue = _internalVisible
            }
        }

        MapScale {
            id:                     mapScale
            anchors.margins:        _toolsMargin
            anchors.bottom:         terrainStatus.visible ? terrainStatus.top : parent.bottom
            anchors.left:           toolStrip.y + toolStrip.height + _toolsMargin > mapScale.y ? toolStrip.right: parent.left
            mapControl:             editorMap
            buttonsOnLeft:          true
            terrainButtonVisible:   _editingLayer === _layerMission
            terrainButtonChecked:   terrainStatus.visible
            onTerrainButtonClicked: terrainStatus.toggleVisible()
        }
    }

    function showLoadFromFileOverwritePrompt(title) {
        mainWindow.showMessageDialog(title,
                                     qsTr("You have unsaved/unsent changes. Loading from a file will lose these changes. Are you sure you want to load from a file?"),
                                     Dialog.Yes | Dialog.Cancel,
                                     function() { _planMasterController.loadFromSelectedFile() } )
    }

    Component {
        id: createPlanRemoveAllPromptDialog

        QGCSimpleMessageDialog {
            title:      qsTr("Create Plan")
            text:       qsTr("Are you sure you want to remove current plan and create a new plan? ")
            buttons:    Dialog.Yes | Dialog.No

            property var mapCenter
            property var planCreator

            onAccepted: planCreator.createPlan(mapCenter)
        }
    }

    Component {
        id: starMissionReplacePromptDialog

        QGCSimpleMessageDialog {
            title:      qsTr("AUV Stars 2026 Mission One")
            // Says what survives, because that is the whole question the operator has:
            // a plan this creator made comes back target for target, anything else does not.
            text:       recovered.length > 0
                            ? qsTr("Replace the current plan?\n\n%1 target(s) will be carried over with their headings and anchor flags. The cruise depth, the holds and the photo timings are not read back — check the boxes before you finish.").arg(recovered.length)
                            : qsTr("Replace the current plan?\n\nIt does not look like an AUV Stars 2026 Mission One plan, so nothing can be carried over and it will be lost.")
            buttons:    Dialog.Yes | Dialog.No

            property var planCreator
            property var recovered: []

            onAccepted: _startStarMissionMode(planCreator)
        }
    }

    Component {
        id: starMissionOneDialog

        QGCPopupDialog {
            title:      qsTr("AUV Stars 2026 Mission One")
            buttons:    Dialog.Ok | Dialog.Cancel

            property var    planCreator
            property var    mapCenter
            // What is already placed on the map. The table opens on top of it instead
            // of discarding it: rough it out by clicking, then refine here.
            property var    startCoord:     null    ///< placed start point, null = nothing placed yet
            property var    seedTargets:    []      ///< placed targets, in order
            // Seeded from the banner so opening the table keeps what was typed there.
            property string depthText:      ""
            property bool   autoDepth:      false
            property string surfaceText:    ""
            property string bottomText:     ""
            property bool   anchorAll:      false   ///< banner "Anchor" seeds every row's checkbox
            property bool   startAnchor:    false   ///< banner "Start anchor" seeds the Home row checkbox
            property string photoBeforeText: ""     ///< CONDITION_DELAY before the shutter (s)
            property string photoWindowText: ""     ///< hold on the photo-window waypoint (s)
            property int    wpCount:    0       ///< rows; sized on completion, grown by "+ Waypoint"

            // Size the table only once createObject has applied seedTargets. Binding
            // wpCount to seedTargets.length instead would race: the rows get built
            // during creation, while seedTargets is still the empty default, so their
            // Component.onCompleted could seed from nothing. Always one spare blank row.
            Component.onCompleted: wpCount = Math.max(3, seedTargets.length + 1)

            property real _labelWidth:  ScreenTools.defaultFontPixelWidth * 12
            property real _latLonWidth: ScreenTools.defaultFontPixelWidth * 12
            property real _yawWidth:    ScreenTools.defaultFontPixelWidth * 7

            function _coord(latText, lonText) {
                return QtPositioning.coordinate(parseFloat(latText), parseFloat(lonText))
            }


            onAccepted: {
                var targets = []
                for (var i = 0; i < wpRepeater.count; i++) {
                    var row = wpRepeater.itemAt(i)
                    // Rows added with "+ Waypoint" but left empty are simply skipped.
                    if (!row || row.latText.length === 0 || row.lonText.length === 0) {
                        continue
                    }
                    targets.push({ coordinate: _coord(row.latText, row.lonText),
                                   camera:     row.camOn,
                                   yaw:        (row.camOn && row.yawText.length > 0) ? parseFloat(row.yawText) : -1,
                                   anchor:     row.anchorOn })
                }
                if (targets.length === 0) {
                    return
                }
                var depth = _starMissionDepth(dialogDepthField.text)
                _exitStarMissionMode()
                planCreator.createFullPlan(_coord(homeLat.text, homeLon.text), targets, depth,
                                           autoDepthCheck.checked,
                                           _starMissionClearance(surfaceClearanceField.text),
                                           _starMissionClearance(bottomClearanceField.text),
                                           _starMissionWait(photoBeforeField.text),
                                           _starMissionWait(photoWindowField.text),
                                           homeAnchorCheck.checked)
            }

            ColumnLayout {
                spacing: ScreenTools.defaultFontPixelHeight / 2

                RowLayout {
                    spacing: ScreenTools.defaultFontPixelWidth

                    QGCLabel { text: qsTr("Position"); font.bold: true; Layout.preferredWidth: _labelWidth }
                    QGCLabel { text: qsTr("Lat");      font.bold: true; Layout.preferredWidth: _latLonWidth }
                    QGCLabel { text: qsTr("Lon");      font.bold: true; Layout.preferredWidth: _latLonWidth }
                    QGCLabel { text: qsTr("Camera");   font.bold: true }
                    QGCLabel { text: qsTr("Yaw (°)");  font.bold: true }
                    QGCLabel { text: qsTr("Anchor");   font.bold: true }
                }

                RowLayout {
                    spacing: ScreenTools.defaultFontPixelWidth

                    QGCLabel     { text: qsTr("Home"); Layout.preferredWidth: _labelWidth }
                    // Prefer the start point already placed on the map; the map centre is
                    // only a fallback for opening the table without placing anything.
                    QGCTextField { id: homeLat; Layout.preferredWidth: _latLonWidth
                                   text: startCoord ? startCoord.latitude.toFixed(7)  : (mapCenter ? mapCenter.latitude.toFixed(7)  : "") }
                    QGCTextField { id: homeLon; Layout.preferredWidth: _latLonWidth
                                   text: startCoord ? startCoord.longitude.toFixed(7) : (mapCenter ? mapCenter.longitude.toFixed(7) : "") }
                    // Start anchor: dropped once at the start point, after the dive in
                    // place and before the first travel leg. It is a departure gate, not
                    // a photo point — no camera turn and no shutter here.
                    QGCCheckBox  { id: homeAnchorCheck; text: qsTr("Start anchor"); checked: startAnchor }
                }

                Repeater {
                    id:     wpRepeater
                    model:  wpCount

                    RowLayout {
                        spacing: ScreenTools.defaultFontPixelWidth

                        // Read back by onAccepted; the row keeps its own state so
                        // adding a waypoint never disturbs the ones already typed.
                        property alias latText:   latField.text
                        property alias lonText:   lonField.text
                        property alias camOn:     camCheck.checked
                        property alias yawText:   yawField.text
                        property alias anchorOn:  anchorCheck.checked

                        // Fill in what is already placed on the map, including anything
                        // carried over from an existing plan. Assigned rather than bound
                        // so typing over a seeded value sticks.
                        Component.onCompleted: {
                            if (index < seedTargets.length) {
                                var t = seedTargets[index]
                                latField.text      = t.coordinate.latitude.toFixed(7)
                                lonField.text      = t.coordinate.longitude.toFixed(7)
                                camCheck.checked   = t.camera
                                anchorCheck.checked = t.anchor
                                // Negative means "no turn" all the way down to the
                                // firmware, so it shows as an empty (auto) box.
                                yawField.text      = t.yaw >= 0 ? String(t.yaw) : ""
                            }
                        }

                        QGCLabel     { text: qsTr("Waypoint %1").arg(index + 1); Layout.preferredWidth: _labelWidth }
                        QGCTextField { id: latField;  Layout.preferredWidth: _latLonWidth; placeholderText: qsTr("Lat") }
                        QGCTextField { id: lonField;  Layout.preferredWidth: _latLonWidth; placeholderText: qsTr("Lon") }
                        QGCCheckBox  { id: camCheck;  text: qsTr("On"); checked: true }
                        QGCTextField { id: yawField;  Layout.preferredWidth: _yawWidth; enabled: camCheck.checked; placeholderText: qsTr("auto") }
                        // Anchor: the vehicle drops anchor at this location — it locks the
                        // point and does not advance to the next waypoint until it has stayed
                        // inside the settle radius continuously, and the anchor command runs
                        // the camera turn and the shutter itself. Requires the aurapilot
                        // fork's MAV_CMD_AURA_ANCHOR (31010) support.
                        QGCCheckBox  { id: anchorCheck; text: qsTr("On"); checked: anchorAll }
                    }
                }

                RowLayout {
                    spacing:            ScreenTools.defaultFontPixelWidth
                    Layout.fillWidth:   true

                    QGCButton {
                        text:       qsTr("+ Waypoint")
                        onClicked:  wpCount++
                    }

                    QGCButton {
                        text:       qsTr("- Waypoint")
                        enabled:    wpCount > 1
                        onClicked:  wpCount--
                    }

                    Item { Layout.fillWidth: true }

                    QGCLabel { text: qsTr("Depth (m)") }

                    QGCTextField {
                        id:                     dialogDepthField
                        Layout.preferredWidth:  _yawWidth + ScreenTools.defaultFontPixelWidth * 2
                        text:                   depthText
                        placeholderText:        qsTr("-1.0")
                        enabled:                !autoDepthCheck.checked
                        // Empty = -1 m; a positive number is read as depth (_starMissionDepth).
                        // Bottom following overrides it, so the box is greyed out then.
                    }
                }

                // Automatic depth control: cruise legs ride the bottom in the terrain
                // frame (rangefinder) instead of holding a fixed barometric depth.
                // Needs RNGFND1_TYPE/ORIENT/MAX_CM + WP_RFND_USE=1 and a live
                // DISTANCE_SENSOR stream on the vehicle, otherwise AUTO throws a
                // terrain failsafe. Surfacing for photos stays barometric.
                RowLayout {
                    spacing:            ScreenTools.defaultFontPixelWidth
                    Layout.fillWidth:   true

                    QGCCheckBox {
                        id:         autoDepthCheck
                        text:       qsTr("Auto depth (rangefinder + baro)")
                        checked:    autoDepth
                    }

                    Item { Layout.fillWidth: true }

                    QGCLabel {
                        text:       qsTr("From surface (m)")
                        enabled:    autoDepthCheck.checked
                    }

                    QGCTextField {
                        id:                     surfaceClearanceField
                        Layout.preferredWidth:  _yawWidth
                        text:                   surfaceText
                        placeholderText:        qsTr("0.3")
                        enabled:                autoDepthCheck.checked
                        // Shallowest a cruise leg may be commanded to (m below surface).
                    }

                    QGCLabel {
                        text:       qsTr("From bottom (m)")
                        enabled:    autoDepthCheck.checked
                    }

                    QGCTextField {
                        id:                     bottomClearanceField
                        Layout.preferredWidth:  _yawWidth
                        text:                   bottomText
                        placeholderText:        qsTr("1.0")
                        enabled:                autoDepthCheck.checked
                        // Clearance held above the sea floor while cruising (m).
                    }
                }

                // Foto zamanlaması: üretilen plandaki iki bekleme. Boş bırakılırsa
                // varsayılanlar (3 / 5 sn) kullanılır — elle doğrulanmış planla aynı.
                RowLayout {
                    spacing:            ScreenTools.defaultFontPixelWidth
                    Layout.fillWidth:   true

                    QGCLabel { text: qsTr("Foto öncesi (sn)") }

                    QGCTextField {
                        id:                     photoBeforeField
                        Layout.preferredWidth:  _yawWidth
                        text:                   photoBeforeText
                        placeholderText:        _starMissionSettings.photoBefore.valueString
                        // CONDITION_DELAY before the shutter. Empty = the App Settings ->
                        // Mission One value shown as the placeholder.
                    }

                    Item { Layout.fillWidth: true }

                    QGCLabel { text: qsTr("Foto penceresi (sn)") }

                    QGCTextField {
                        id:                     photoWindowField
                        Layout.preferredWidth:  _yawWidth
                        text:                   photoWindowText
                        placeholderText:        _starMissionSettings.photoWindow.valueString
                        // Hold time of the photo-window waypoint (anchor duration when Anchor
                        // is on). 2 s is added on top when the camera turns; it cannot be
                        // shorter than the queue, otherwise AP_Mission drops the queue and no
                        // photo is taken.
                    }
                }
            }
        }
    }

    function clearButtonClicked() {
        mainWindow.showMessageDialog(qsTr("Clear"),
                                     qsTr("Are you sure you want to remove all mission items and clear the mission from the vehicle?"),
                                     Dialog.Yes | Dialog.Cancel,
                                     function() { _planMasterController.removeAllFromVehicle();
                                                  _missionController.setCurrentPlanViewSeqNum(0, true);
                                                  if(_utmspEnabled)
                                                    {_resetRegisterFlightPlan = true;
                                                      QGroundControl.utmspManager.utmspVehicle.triggerActivationStatusBar(false);
                                                      UTMSPStateStorage.startTimeStamp = "";
                                                      UTMSPStateStorage.showActivationTab = false;
                                                      UTMSPStateStorage.flightID = "";
                                                      UTMSPStateStorage.enableMissionUploadButton = false;
                                                      UTMSPStateStorage.indicatorPendingStatus = true;
                                                      UTMSPStateStorage.indicatorApprovedStatus = false;
                                                      UTMSPStateStorage.indicatorActivatedStatus = false;
                                                      UTMSPStateStorage.currentStateIndex = 0}})
    }

    //- ToolStrip ToolStripDropPanel Components

    Component {
        id: centerMapDropPanel

        CenterMapDropPanel {
            map:            editorMap
            fitFunctions:   mapFitFunctions
        }
    }

    Component {
        id: patternDropPanel

        ColumnLayout {
            spacing:    ScreenTools.defaultFontPixelWidth * 0.5

            QGCLabel { text: qsTr("Create complex pattern:") }

            Repeater {
                model: _missionController.complexMissionItemNames

                QGCButton {
                    text:               modelData
                    Layout.fillWidth:   true

                    onClicked: {
                        insertComplexItemAfterCurrent(modelData)
                        dropPanel.hide()
                    }
                }
            }
        } // Column
    }

    function downloadClicked(title) {
        if (_planMasterController.dirty) {
            mainWindow.showMessageDialog(title,
                                         qsTr("You have unsaved/unsent changes. Loading from the Vehicle will lose these changes. Are you sure you want to load from the Vehicle?"),
                                         Dialog.Yes | Dialog.Cancel,
                                         function() { _planMasterController.loadFromVehicle() })
        } else {
            _planMasterController.loadFromVehicle()
        }
    }

    Component {
        id: syncDropPanel

        ColumnLayout {
            id:         columnHolder
            spacing:    _margin

            property string _overwriteText: qsTr("Plan overwrite")

            QGCLabel {
                id:                 unsavedChangedLabel
                Layout.fillWidth:   true
                wrapMode:           Text.WordWrap
                text:               globals.activeVehicle ?
                                        qsTr("You have unsaved changes. You should upload to your vehicle, or save to a file.") :
                                        qsTr("You have unsaved changes.")
                visible:            _planMasterController.dirty
            }

            SectionHeader {
                id:                 createSection
                Layout.fillWidth:   true
                text:               qsTr("Create Plan")
                showSpacer:         false
            }

            GridLayout {
                columns:            2
                columnSpacing:      _margin
                rowSpacing:         _margin
                Layout.fillWidth:   true
                visible:            createSection.checked

                Repeater {
                    model: _planMasterController.planCreators

                    Rectangle {
                        id:             button
                        // Implicit sizes (not width/height) so the layout keeps following
                        // the label: a wrapped two-line name makes the tile taller instead
                        // of being cut off by a height the layout latched onto early.
                        implicitWidth:  ScreenTools.defaultFontPixelHeight * 7
                        implicitHeight: planCreatorNameLabel.y + planCreatorNameLabel.height
                        color:          button.pressed || button.highlighted ? qgcPal.buttonHighlight : qgcPal.button

                        property bool highlighted: mouseArea.containsMouse
                        property bool pressed:     mouseArea.pressed

                        Image {
                            id:                 planCreatorImage
                            anchors.left:       parent.left
                            anchors.right:      parent.right
                            source:             object.imageResource
                            sourceSize.width:   width
                            fillMode:           Image.PreserveAspectFit
                            mipmap:             true
                        }

                        QGCLabel {
                            id:                     planCreatorNameLabel
                            anchors.top:            planCreatorImage.bottom
                            anchors.left:           parent.left
                            anchors.right:          parent.right
                            horizontalAlignment:    Text.AlignHCenter
                            // Long names ("AUV Stars 2026 Mission One") are wider than the
                            // tile and used to paint out over both of its sides; wrap onto
                            // a second line instead and let the tile grow to fit.
                            wrapMode:               Text.WordWrap
                            text:                   object.name
                            color:                  button.pressed || button.highlighted ? qgcPal.buttonHighlightText : qgcPal.buttonText
                        }

                        QGCMouseArea {
                            id:                 mouseArea
                            anchors.fill:       parent
                            hoverEnabled:       true
                            preventStealing:    true
                            onClicked:          {
                                if (object.interactive) {
                                    // The interactive creator wipes the plan on entry, so it
                                    // has to ask like the others do - it used to be the one
                                    // path that destroyed a loaded plan without a word.
                                    if (_planMasterController.containsItems) {
                                        starMissionReplacePromptDialog.createObject(mainWindow, { planCreator: object,
                                                                                                  recovered:   object.extractTargets() }).open()
                                    } else {
                                        _startStarMissionMode(object)
                                    }
                                } else if (_planMasterController.containsItems) {
                                    createPlanRemoveAllPromptDialog.createObject(mainWindow, { mapCenter: _mapCenter(), planCreator: object }).open()
                                } else {
                                    object.createPlan(_mapCenter())
                                }
                                dropPanel.hide()
                            }

                            function _mapCenter() {
                                var centerPoint = Qt.point(editorMap.centerViewport.left + (editorMap.centerViewport.width / 2), editorMap.centerViewport.top + (editorMap.centerViewport.height / 2))
                                return editorMap.toCoordinate(centerPoint, false /* clipToViewPort */)
                            }
                        }
                    }
                }
            }

            SectionHeader {
                id:                 storageSection
                Layout.fillWidth:   true
                text:               qsTr("Storage")
            }

            GridLayout {
                columns:            3
                rowSpacing:         _margin
                columnSpacing:      ScreenTools.defaultFontPixelWidth
                visible:            storageSection.checked

                QGCButton {
                    text:               qsTr("Open...")
                    Layout.fillWidth:   true
                    enabled:            !_planMasterController.syncInProgress
                    onClicked: {
                        dropPanel.hide()
                        if (_planMasterController.dirty) {
                            showLoadFromFileOverwritePrompt(columnHolder._overwriteText)
                        } else {
                            _planMasterController.loadFromSelectedFile()
                        }
                    }
                }

                QGCButton {
                    text:               qsTr("Save")
                    Layout.fillWidth:   true
                    enabled:            !_planMasterController.syncInProgress && _planMasterController.currentPlanFile !== ""
                    onClicked: {
                        dropPanel.hide()
                        if(_planMasterController.currentPlanFile !== "") {
                            _planMasterController.saveToCurrent()
                        } else {
                            _planMasterController.saveToSelectedFile()
                        }
                    }
                }

                QGCButton {
                    text:               qsTr("Save As...")
                    Layout.fillWidth:   true
                    enabled:            !_planMasterController.syncInProgress && _planMasterController.containsItems
                    onClicked: {
                        dropPanel.hide()
                        _planMasterController.saveToSelectedFile()
                    }
                }

                QGCButton {
                    Layout.columnSpan:  3
                    Layout.fillWidth:   true
                    text:               qsTr("Save Mission Waypoints As KML...")
                    enabled:            !_planMasterController.syncInProgress && _visualItems.count > 1
                    onClicked: {
                        // First point does not count
                        if (_visualItems.count < 2) {
                            mainWindow.showMessageDialog(qsTr("KML"), qsTr("You need at least one item to create a KML."))
                            return
                        }
                        dropPanel.hide()
                        _planMasterController.saveKmlToSelectedFile()
                    }
                }
            }

            SectionHeader {
                id:                 vehicleSection
                Layout.fillWidth:   true
                text:               qsTr("Vehicle")
            }

            RowLayout {
                Layout.fillWidth:   true
                spacing:            _margin
                visible:            vehicleSection.checked

                QGCButton {
                    text:               qsTr("Upload")
                    Layout.fillWidth:   true
                    enabled:            !_planMasterController.offline && !_planMasterController.syncInProgress && _planMasterController.containsItems
                    visible:            !QGroundControl.corePlugin.options.disableVehicleConnection
                    onClicked: {
                        dropPanel.hide()
                        _planMasterController.upload()
                    }
                }

                QGCButton {
                    text:               qsTr("Download")
                    Layout.fillWidth:   true
                    enabled:            !_planMasterController.offline && !_planMasterController.syncInProgress
                    visible:            !QGroundControl.corePlugin.options.disableVehicleConnection

                    onClicked: {
                        dropPanel.hide()
                        downloadClicked(columnHolder._overwriteText)
                    }
                }

                QGCButton {
                    text:               qsTr("Clear")
                    Layout.fillWidth:   true
                    Layout.columnSpan:  2
                    enabled:            !_planMasterController.offline && !_planMasterController.syncInProgress
                    visible:            !QGroundControl.corePlugin.options.disableVehicleConnection
                    onClicked: {
                        dropPanel.hide()
                        clearButtonClicked()
                    }
                }
            }
        }
    }

    Connections {
        target: utmspEditor
        function onVehicleIDSent(id) {
            _vehicleID = id
        }
    }
    Connections {
        target: utmspEditor
        function onRemoveFlightPlanTriggered() {
            _planMasterController.removeAllFromVehicle();
            _missionController.setCurrentPlanViewSeqNum(0, true);
            if(_utmspEnabled){_resetRegisterFlightPlan = true}
        }
    }
}
