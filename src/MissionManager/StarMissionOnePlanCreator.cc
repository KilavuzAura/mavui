/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "StarMissionOnePlanCreator.h"
#include "PlanMasterController.h"
#include "MissionController.h"
#include "SimpleMissionItem.h"
#include "MissionItem.h"
#include "QmlObjectListModel.h"
#include "SettingsManager.h"
#include "StarMissionSettings.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QVariantMap>
#include <QtCore/QTemporaryFile>
#include <QtCore/QDir>

#include <algorithm>
#include <cmath>

const QString StarMissionOnePlanCreator::name = QStringLiteral("AUV Stars 2026 Mission One");

namespace {
// Movement pattern constants — kept in sync with tools/aura_foto_plan_uret.py.
// The cruise depth is no longer fixed: it comes from the UI (banner / table box)
// and falls back to StarMissionOnePlanCreator::kDefaultCruiseDepth (-1 m).
// Surface waypoints are -0.1 m, NOT 0. Frame 3 altitudes are relative to HOME,
// and home is captured where the vehicle floats, so alt = 0 asks for a target at
// (or above) the waterline: the sub can never reach it, pins the vertical
// thrusters at full and hovers ~0.4 m down, taking the photo underwater. -0.1 m
// also keeps the depth sensor submerged (hull ~35 cm, sensor at mid height) while
// the camera on top clears the water. Matches aura_foto_plan_uret.py /
// gorev_yukle.py (SATIH_DERINLIK).
constexpr double kSurfaceDepth  = -0.1; // surface waypoint depth (m, negative)
constexpr int    kTurnSeconds   = 2;    // nominal padding added to both window and queue
// Worst-case camera turn, used only on the unanchored path to size the photo window.
// Not a nominal figure: it has to cover the slowest turn the vehicle can make, because
// the window running out silently kills the shutter. The firmware's own anchor guard
// allows 30 s for a turn; 20 s covers every turn measured on the vehicle with room to
// spare (131 deg reached the +-2 deg window in 3.8 s, and settled continuously by 16 s).
constexpr int    kTurnBudgetSeconds = 20;
constexpr int    kYawRate       = 90;   // camera yaw rate (deg/s)

// The dive/surface holds and the "drop anchor" (MAV_CMD_AURA_ANCHOR) tuning are
// operator-editable in App Settings -> Mission One (StarMissionSettings), so they
// are read per plan instead of being compile-time constants. Both matter because
// AC_WPNav latches reached_destination (AC_WPNav.cpp:540): once the sub touches
// WPNAV_RADIUS the hold timer runs even if drift then carries it away, and a
// vertical leg completes while the sub is still shallow.
struct MissionTuning {
    int    startWait;       // seconds to hold the start point before the position fix
    int    diveSettle;      // seconds to settle after diving
    int    surfaceSettle;   // seconds to settle after surfacing
    double anchorRadius;    // settle radius (m); 0 disables the settle gate
    int    anchorSettle;    // seconds to stay inside the radius, uninterrupted
    int    anchorGuard;     // safety ceiling (s); 0 = firmware default (duration*3 + 30)
    double photoBefore;     // CONDITION_DELAY before the shutter (s)
    double photoWindow;     // hold on the photo-window waypoint / anchor duration (s)
};

MissionTuning loadTuning()
{
    StarMissionSettings* s = SettingsManager::instance()->starMissionSettings();
    return {
        s->startWait()->rawValue().toInt(),
        s->diveSettle()->rawValue().toInt(),
        s->surfaceSettle()->rawValue().toInt(),
        s->anchorRadius()->rawValue().toDouble(),
        s->anchorSettle()->rawValue().toInt(),
        s->anchorGuard()->rawValue().toInt(),
        s->photoBefore()->rawValue().toDouble(),
        s->photoWindow()->rawValue().toDouble(),
    };
}

constexpr int kCmdNavWaypoint       = 16;
constexpr int kCmdConditionDelay    = 112;
constexpr int kCmdConditionYaw      = 115;
constexpr int kCmdDoDigicamControl  = 203;
constexpr int kCmdAuraAnchor        = 31010; // MAV_CMD_USER_1 in the aurapilot ArduSub fork
constexpr int kCmdAuraPositionFix   = 31015; // MAV_CMD_AURA_POSITION_FIX in the same fork
// Position-fix dwell (param1). 0 is not "advance immediately": the firmware reads it as
// its own 1 s default, which is what the reset needs to travel through the estimator,
// AP_InertialNav and the position controller before the next leg is computed from it.
constexpr int kPositionFixDwell     = 0;
// Anchor param5 (heading, whole degrees): negative means "keep the current heading".
// 0 is due north, so an unset slot must be -1, never 0.
constexpr int kNoYaw                = -1;
// Anchor param7 (pre-shutter wait) is a 5-bit field in the mission command, so the
// firmware clamps anything above this. Clamp here too rather than let a value the
// operator typed be silently cut down on the vehicle.
constexpr int kMaxAnchorPhotoDelay  = 31;
constexpr int kFrameGlobalRelativeAlt = 3;
constexpr int kFrameGlobalTerrainAlt  = 10;
constexpr int kFrameMission           = 2;
// QGroundControlQmlGlobal::AltMode values as written into the .plan file.
constexpr int kAltModeRelative      = 1;
constexpr int kAltModeTerrainFrame  = 4;

class PatternBuilder
{
public:
    // autoDepth: cruise legs are emitted in the terrain frame (rangefinder holds
    // bottomClearance above the floor) instead of the fixed barometric depth.
    PatternBuilder(double cruiseDepth, bool autoDepth, double bottomClearance)
        : _cruiseDepth(cruiseDepth), _autoDepth(autoDepth), _bottomClearance(bottomClearance) {}

    // Cruise waypoint: depth below the surface, or clearance above the floor
    // when bottom following is on. Mirrors gorev_yukle.py seyir_wp().
    void cruise(double lat, double lon, int hold = 0) {
        if (_autoDepth) {
            _waypoint(lat, lon, _bottomClearance, hold, kFrameGlobalTerrainAlt, kAltModeTerrainFrame);
        } else {
            waypoint(lat, lon, _cruiseDepth, hold);
        }
    }

    // Surface/absolute-depth waypoint (frame 3). Surfacing is always barometric:
    // the rangefinder says nothing about how close the hull is to the waterline.
    void waypoint(double lat, double lon, double alt, int hold = 0) {
        _waypoint(lat, lon, alt, hold, kFrameGlobalRelativeAlt, kAltModeRelative);
    }

    // A DO/CONDITION command item (frame 2, no coordinate).
    void command(int cmd, const QJsonArray& params) {
        QJsonObject o;
        o[QStringLiteral("autoContinue")] = true;
        o[QStringLiteral("command")]      = cmd;
        o[QStringLiteral("doJumpId")]     = _nextId();
        o[QStringLiteral("frame")]        = kFrameMission;
        o[QStringLiteral("params")]       = params;
        o[QStringLiteral("type")]         = QStringLiteral("SimpleItem");
        _items.append(o);
    }

    QJsonArray items() const { return _items; }

private:
    void _waypoint(double lat, double lon, double alt, int hold, int frame, int altMode) {
        QJsonObject o;
        o[QStringLiteral("AMSLAltAboveTerrain")] = QJsonValue::Null;
        o[QStringLiteral("Altitude")]            = alt;
        o[QStringLiteral("AltitudeMode")]        = altMode;
        o[QStringLiteral("autoContinue")]        = true;
        o[QStringLiteral("command")]             = kCmdNavWaypoint;
        o[QStringLiteral("doJumpId")]            = _nextId();
        o[QStringLiteral("frame")]               = frame;
        o[QStringLiteral("params")]              = QJsonArray({ hold, 0, 0, QJsonValue::Null, lat, lon, alt });
        o[QStringLiteral("type")]                = QStringLiteral("SimpleItem");
        _items.append(o);
    }

    int _nextId() { return _jumpId++; }

    QJsonArray _items;
    int        _jumpId = 1;
    double     _cruiseDepth;
    bool       _autoDepth;
    double     _bottomClearance;
};
} // namespace

StarMissionOnePlanCreator::StarMissionOnePlanCreator(PlanMasterController* planMasterController, QObject* parent)
    : PlanCreator(planMasterController, name, QStringLiteral("/qmlimages/PlanCreator/BlankPlanCreator.png"), parent)
{
    _interactive = true;
}

void StarMissionOnePlanCreator::createPlan(const QGeoCoordinate& /*mapCenterCoord*/)
{
    // Interactive: the real work happens once the placement mode has the coordinates.
}

void StarMissionOnePlanCreator::createFullPlan(const QGeoCoordinate&  home,
                                               const QVariantList&    targets,
                                               double                 cruiseDepth,
                                               bool                   autoDepth,
                                               double                 surfaceClearance,
                                               double                 bottomClearance,
                                               double                 photoBefore,
                                               double                 photoWindow,
                                               bool                   startAnchor,
                                               double                 startWait,
                                               double                 diveSettle,
                                               bool                   returnHome)
{
    if (!home.isValid() || targets.isEmpty()) {
        return;
    }

    // Depth must be negative (below surface). A blank/zero/positive box means
    // "use the default" rather than driving the vehicle above the waterline.
    if (!std::isfinite(cruiseDepth) || cruiseDepth >= 0.0) {
        cruiseDepth = kDefaultCruiseDepth;
    }

    // Both clearances are distances, so they are always positive. A blank or
    // nonsense box falls back to the defaults instead of producing a plan that
    // asks for a target at (or above) the waterline / inside the sea floor.
    if (!std::isfinite(surfaceClearance) || surfaceClearance <= 0.0) {
        surfaceClearance = kDefaultSurfaceClearance;
    }
    // Bounded in both directions, because this box ends up setting the cruise depth
    // (see the clamp further down) and the UI applies no validator.
    //   Too small: a clearance of 0.1 pulls the cruise legs to -0.1 m, the same depth
    //   as the surface waypoints. The pattern collapses - the vehicle travels at the
    //   surface - and extractTargets() then reads every cruise leg back as a target,
    //   because a surface waypoint is recognised by its depth.
    //   Too large: someone typing 30 (thinking centimetres) sends the vehicle to 30 m.
    surfaceClearance = std::clamp(surfaceClearance, kMinSurfaceClearance, kMaxSurfaceClearance);
    if (!std::isfinite(bottomClearance) || bottomClearance <= 0.0) {
        bottomClearance = kDefaultBottomClearance;
    }

    const MissionTuning tuning = loadTuning();

    // Photo timing boxes. A blank/nonsense box falls back to whatever the operator
    // set in App Settings -> Mission One, so the panel is the single source of truth
    // for the defaults; the per-plan boxes only override them.
    if (!std::isfinite(photoBefore) || photoBefore < 0.0) {
        photoBefore = tuning.photoBefore;
    }
    if (!std::isfinite(photoWindow) || photoWindow <= 0.0) {
        photoWindow = tuning.photoWindow;
    }
    const int photoBeforeS = static_cast<int>(std::lround(photoBefore));
    const int photoWindowS = static_cast<int>(std::lround(photoWindow));

    // The two hold boxes work the same way: blank/nonsense means "use App Settings",
    // but an explicit 0 is a value in its own right and is kept. For the start hold 0
    // also switches the whole start gate off, and for the dive hold it means "trust
    // WPNAV_RADIUS", so neither may be swallowed by the fallback.
    if (!std::isfinite(startWait) || startWait < 0.0) {
        startWait = tuning.startWait;
    }
    if (!std::isfinite(diveSettle) || diveSettle < 0.0) {
        diveSettle = tuning.diveSettle;
    }
    const int startWaitS  = static_cast<int>(std::lround(startWait));
    const int diveSettleS = static_cast<int>(std::lround(diveSettle));

    // Never cruise shallower than the surface clearance: the cruise legs are the
    // only part of the pattern that runs blind, and a too-shallow leg puts the
    // hull (and the mission) at the waterline. Surfacing for a photo is a
    // separate, deliberate -0.1 m waypoint and is not clamped.
    if (cruiseDepth > -surfaceClearance) {
        cruiseDepth = -surfaceClearance;
    }

    PatternBuilder b(cruiseDepth, autoDepth, bottomClearance);
    double prevLat = home.latitude();
    double prevLon = home.longitude();
    bool   first   = true;

    // 0) Start gate: fix, then - if a hold was asked for - hold and fix again.
    //    The vehicle is driven onto the start marker by hand, and the hold is an anchor
    //    rather than a waypoint because an anchor locks where the vehicle IS - a waypoint
    //    would first fly to where the drifted solution thinks the start is, which is the
    //    error this gate exists to remove.
    //    The FIRST MAV_CMD_AURA_POSITION_FIX runs before the hold. Measured on the
    //    vehicle (20 Aug, 00000458 t=326 and 00000457 t=387): the solution was ~20 m off
    //    when AUTO was engaged, so an anchor running first held - and its settle gate
    //    certified - a point 20 m away from the one the plan calls the start, and the
    //    correction only landed once the hold was over. The gate exists to prove the
    //    vehicle is holding the START, so the correction has to come first. It also puts
    //    the fix on mission item 1, where the operator can re-apply it from the GCS with
    //    "set current item" while still in MANUAL and watch the map snap before
    //    committing to AUTO (seen at t=617.8 and t=700.2 in the same log).
    //    The SECOND fix is the one that has to come after: snapping while the vehicle is
    //    still gliding writes that glide back into the solution, and the hold is exactly
    //    the window in which the glide stops.
    //    Both read the coordinate of the next nav item that stores a Location - the dive
    //    in place emitted right below, which carries the start coordinate. The anchor
    //    between them carries none and is stepped over (Sub::do_position_fix walks
    //    forward past location-less nav commands). On firmware without that walk the
    //    LEADING fix is skipped with "no waypoint after it" and the gate degrades to the
    //    old hold-then-fix - which is why hand-edited plans put a dummy waypoint there,
    //    with a positive altitude so ArduSub would reject it, purely to carry the
    //    coordinate. Nothing else depends on it, so no dummy is emitted here.
    //    Firmware note: the fix is only accepted while dead reckoning. With a GPS-class
    //    source still fusing (UGPS, GPS_INPUT, SITL's fake GPS) it is rejected, says so
    //    over STATUSTEXT, and the mission carries on unchanged.
    //    The LEADING fix is emitted whatever the hold is, zero included. Snapping the
    //    solution onto the start coordinate is worth doing on its own - it is the whole
    //    reason the operator was asked to drive onto the marker - and a plan with no
    //    hold is exactly the plan that gets no other chance to correct before the first
    //    dive. What 0 drops is the hold and, with it, the trailing fix: with no hold
    //    there is no glide-stopping window for a second snap to sit at the end of.
    b.command(kCmdAuraPositionFix, QJsonArray({ kPositionFixDwell, 0, 0, 0, 0, 0, 0 }));
    if (startWaitS > 0) {
        b.command(kCmdAuraAnchor, QJsonArray({ startWaitS, tuning.anchorRadius,
                                               tuning.anchorSettle, tuning.anchorGuard,
                                               kNoYaw, 0, 0 }));
        b.command(kCmdAuraPositionFix, QJsonArray({ kPositionFixDwell, 0, 0, 0, 0, 0, 0 }));
    }

    for (const QVariant& targetVar : targets) {
        const QVariantMap target = targetVar.toMap();
        const QGeoCoordinate coord = target.value(QStringLiteral("coordinate")).value<QGeoCoordinate>();
        if (!coord.isValid()) {
            continue;
        }
        const double lat = coord.latitude();
        const double lon = coord.longitude();
        const bool   camera = target.value(QStringLiteral("camera")).toBool();
        const double yaw    = target.value(QStringLiteral("yaw"), -1.0).toDouble();
        const bool   anchor = target.value(QStringLiteral("anchor")).toBool();

        b.cruise(prevLat, prevLon, diveSettleS);        // 1) dive in place and settle at cruise depth
        if (first && startAnchor) {
            // 1b) Start anchor: a departure gate, not a station. Duration 0 means the
            //     settle gate alone decides — as soon as the vehicle has held the start
            //     point for the settle time it leaves. Without it the first travel leg
            //     begins from wherever the dive drifted to, and every later target
            //     inherits that error.
            b.command(kCmdAuraAnchor, QJsonArray({ 0, tuning.anchorRadius,
                                                   tuning.anchorSettle, tuning.anchorGuard,
                                                   kNoYaw, 0, 0 }));
        }
        first = false;
        b.cruise(lat, lon);                             // 2) travel to the target at depth

        const int heading = (camera && yaw >= 0.0)
                                ? (static_cast<int>(yaw) % 360 + 360) % 360
                                : kNoYaw;

        if (anchor) {
            // 3) Anchor at depth, on the target. Two things happen here rather than at
            //    the surface: the settle gate closes out the travel error while the
            //    vehicle is still submerged, and the camera turn runs here, where the
            //    thrusters have full authority and the hull is nowhere near the
            //    waterline. The vehicle reaches the surface already aimed.
            b.command(kCmdAuraAnchor, QJsonArray({ 0, tuning.anchorRadius,
                                                   tuning.anchorSettle, tuning.anchorGuard,
                                                   heading, 0, 0 }));
            // 4) Straight up. Same coordinate as the anchor it just held, so the leg
            //    has no horizontal length and the only motion at the surface is
            //    vertical - the one rule this pattern exists to keep.
            b.waypoint(lat, lon, kSurfaceDepth, tuning.surfaceSettle);
            // 5) Anchor again at the surface and shoot. The vehicle still holds its
            //    point (that is what an anchor does, and holding station is not the
            //    horizontal travel the pattern forbids), so the frame is taken where
            //    the plan asked for it. Re-stating the heading costs nothing - the
            //    turn is already done - but it re-arms the +-2 deg gate in
            //    verify_anchor(), so an ascent that nudged the nose cannot produce an
            //    off-aim frame. Spelling the shutter out as CONDITION_YAW +
            //    CONDITION_DELAY + DO_DIGICAM_CONTROL beside an anchor only worked
            //    when the queue finished inside the hold -- AP_Mission drops a pending
            //    queue the moment the nav command completes, so a short hold silently
            //    meant no photo. Inside the anchor the order is guaranteed and nothing
            //    has to be padded to fit, so photoWindow is the plain hold after the
            //    shutter.
            b.command(kCmdAuraAnchor, QJsonArray({ photoWindowS, tuning.anchorRadius,
                                                   tuning.anchorSettle, tuning.anchorGuard,
                                                   heading, camera ? 1 : 0,
                                                   std::min(photoBeforeS, kMaxAnchorPhotoDelay) }));
        } else if (camera) {
            b.waypoint(lat, lon, kSurfaceDepth, tuning.surfaceSettle);  // 3) surface and settle
            // No anchor: the vehicle is not held on the point, so the shutter still
            // has to be assembled from separate items and the window padded to
            // outlast them.
            int window = photoWindowS;
            int queue  = photoBeforeS;
            if (heading != kNoYaw) {
                b.command(kCmdConditionYaw, QJsonArray({ heading, kYawRate, 0, 0, 0, 0, 0 })); // 4) turn
                window += kTurnSeconds;
                queue  += kTurnSeconds;
            }
            window = std::max(window, queue + 1);
            // kTurnSeconds is a fiction and the padding above is not enough on its own.
            // The rate in the CONDITION_YAW item never reaches the controller: ArduSub
            // clamps it to AUTO_YAW_SLEW_RATE (60 deg/s) and then only reads that value
            // in GUIDED - in AUTO the turn runs at ATC_SLEW_YAW and the body lags well
            // behind it. verify_yaw() closes on measured heading (+-2 deg), not on a
            // duration. Measured on the vehicle (30 Jul log_266): a 64 deg turn first
            // touched the window at 2.5 s, a 131 deg turn at 3.8 s - against a 4.0 s
            // budget. That is a 0.2 s margin, and past ~140 deg it goes negative: the
            // nav command finishes first, advance_current_nav_cmd drops the pending
            // queue and the shutter never fires, with nothing in the log to say so
            // (29 Jul: 6 CondYaw, 0 DigiCamCtrl). The firmware's own auto guard budgets
            // 30 s for a turn; match that order of magnitude rather than 2 s.
            if (heading != kNoYaw) {
                window = std::max(window, photoBeforeS + kTurnBudgetSeconds + 1);
            }

            b.command(kCmdConditionDelay, QJsonArray({ photoBeforeS, 0, 0, 0, 0, 0, 0 })); // 5) wait
            b.command(kCmdDoDigicamControl, QJsonArray({ 0, 0, 0, 0, 1, 0, 0 }));          // 6) take photo
            b.waypoint(lat, lon, kSurfaceDepth, window);                                   // 7) hold the window
        } else {
            // Neither anchored nor photographed: the stop is just a surfacing.
            b.waypoint(lat, lon, kSurfaceDepth, tuning.surfaceSettle);
        }

        prevLat = lat;
        prevLon = lon;
    }

    // Return leg. Optional, and off by default: the mission otherwise ends on the last
    // stop, with the vehicle holding at the surface where it took the final photo. The
    // rule is "after the LAST target", not "after the third" - the pattern takes any
    // number of them.
    if (returnHome) {
        b.cruise(prevLat, prevLon, diveSettleS);         // dive in place and settle at the last target
        b.cruise(home.latitude(), home.longitude());     // return to home underwater
        b.waypoint(home.latitude(), home.longitude(), kSurfaceDepth); // surface, mission complete
    }

    QJsonObject mission;
    mission[QStringLiteral("cruiseSpeed")]            = 15;
    mission[QStringLiteral("firmwareType")]           = 3;  // ArduPilot
    mission[QStringLiteral("globalPlanAltitudeMode")] = 0;
    mission[QStringLiteral("hoverSpeed")]             = 5;
    mission[QStringLiteral("items")]                  = b.items();
    mission[QStringLiteral("plannedHomePosition")]    = QJsonArray({ home.latitude(), home.longitude(), 0 });
    mission[QStringLiteral("vehicleType")]            = 12; // Submarine
    mission[QStringLiteral("version")]                = 2;

    QJsonObject geoFence;
    geoFence[QStringLiteral("circles")]  = QJsonArray();
    geoFence[QStringLiteral("polygons")] = QJsonArray();
    geoFence[QStringLiteral("version")]  = 2;

    QJsonObject rallyPoints;
    rallyPoints[QStringLiteral("points")]  = QJsonArray();
    rallyPoints[QStringLiteral("version")] = 2;

    QJsonObject plan;
    plan[QStringLiteral("fileType")]      = QStringLiteral("Plan");
    plan[QStringLiteral("geoFence")]      = geoFence;
    plan[QStringLiteral("groundStation")] = QStringLiteral("QGroundControl");
    plan[QStringLiteral("mission")]       = mission;
    plan[QStringLiteral("rallyPoints")]   = rallyPoints;
    plan[QStringLiteral("version")]       = 1;

    // Reuse QGC's own plan loader for guaranteed parity with a hand-written .plan.
    QTemporaryFile tempFile(QDir::temp().filePath(QStringLiteral("StarMissionOne-XXXXXX.plan")));
    tempFile.setAutoRemove(true);
    if (!tempFile.open()) {
        return;
    }
    tempFile.write(QJsonDocument(plan).toJson());
    tempFile.flush();
    const QString tempPath = tempFile.fileName();
    tempFile.close();

    _planMasterController->loadFromFile(tempPath);
}

namespace {
// A stop is marked by its surface waypoint. Nothing else in the pattern lands near
// kSurfaceDepth: createFullPlan() clamps the cruise depth to at most -surfaceClearance
// (-0.3 m by default) and terrain-frame cruise legs carry a positive clearance. The
// window is wide enough to also take plans written with -0.09 by hand or by an older
// generator.
// The anchor's x/y carry a heading and a shutter flag, not a coordinate - but only a
// plan read back from a .plan file says so. A plan downloaded from the vehicle arrives
// with frame 0: ArduPilot stamps a frame only on the commands stored_in_location()
// lists (AP_Mission.cpp:1556 leaves packet.frame = 0 otherwise) and the anchor is not
// one of them. QGC then treats x/y of anything that is not MAV_FRAME_MISSION as a
// scaled coordinate and divides by 1e7 (PlanManager.cc:406), so a 90 deg heading
// arrives as 9e-06. Read as degrees that is 0 - due north - which is a heading the
// vehicle really would slew to, at every stop, with nothing on screen to say so.
// Scale it back so both sources give the same answer.
double anchorField(SimpleMissionItem* item, double param)
{
    return item->missionItem().frame() == MAV_FRAME_MISSION ? param : param * 1e7;
}

bool isSurfaceWaypoint(SimpleMissionItem* item)
{
    if (item->command() != kCmdNavWaypoint) {
        return false;
    }
    const double alt = item->altitude()->rawValue().toDouble();
    return alt < 0.0 && std::fabs(alt - kSurfaceDepth) < 0.05;
}
} // namespace

QGeoCoordinate StarMissionOnePlanCreator::extractStart() const
{
    MissionController* missionController = _planMasterController ? _planMasterController->missionController() : nullptr;
    QmlObjectListModel* items = missionController ? missionController->visualItems() : nullptr;
    if (!items) {
        return QGeoCoordinate();
    }

    // Index 0 is the mission settings (Launch) item, which is exactly what must not be
    // trusted here - skip it and take the first item that carries a real coordinate.
    // In our pattern that is the dive in place the plan opens with; the two start-gate
    // commands ahead of it (anchor, position fix) carry none, so they are stepped over
    // without a special case.
    for (int i = 1; i < items->count(); i++) {
        SimpleMissionItem* item = qobject_cast<SimpleMissionItem*>(items->get(i));
        if (item && item->specifiesCoordinate() && item->coordinate().isValid()) {
            return item->coordinate();
        }
    }
    return QGeoCoordinate();
}

QVariantMap StarMissionOnePlanCreator::extractSettings() const
{
    QVariantMap out;
    out[QStringLiteral("valid")] = false;

    MissionController* missionController = _planMasterController ? _planMasterController->missionController() : nullptr;
    QmlObjectListModel* items = missionController ? missionController->visualItems() : nullptr;
    if (!items) {
        return out;
    }
    const int count = items->count();

    // 1) Start gate. Index 0 is the mission settings item; everything between it and the
    //    first coordinate item is the gate (position fix, anchor, position fix - the
    //    leading fix is always there, the other two only when a hold was asked for). The
    //    anchor's duration IS the start hold. No anchor there means the hold was switched
    //    off, and 0 has to be reported as 0 - reporting "unknown" would let the blank box
    //    fall back to the App Settings default and put a hold back into a plan that
    //    deliberately had none.
    int startWait = 0;
    int firstCoordIndex = -1;
    for (int i = 1; i < count; i++) {
        SimpleMissionItem* it = qobject_cast<SimpleMissionItem*>(items->get(i));
        if (!it) {
            break;
        }
        if (it->specifiesCoordinate() && it->coordinate().isValid()) {
            firstCoordIndex = i;
            break;
        }
        if (it->command() == kCmdAuraAnchor) {
            startWait = static_cast<int>(std::lround(it->missionItem().param1()));
        }
    }
    if (firstCoordIndex < 0) {
        return out;     // no coordinate item at all - not a plan this creator wrote
    }

    // 2) The first coordinate item is the dive in place that opens the pattern, so it
    //    carries both the cruise depth (or the bottom clearance in terrain frame) and
    //    the dive hold.
    SimpleMissionItem* dive = qobject_cast<SimpleMissionItem*>(items->get(firstCoordIndex));
    const bool   autoDepth  = dive->missionItem().frame() == kFrameGlobalTerrainAlt;
    const double diveAlt    = dive->altitude()->rawValue().toDouble();
    const int    diveSettle = static_cast<int>(std::lround(dive->missionItem().param1()));

    // 3) Start anchor: the departure gate sits between the dive in place and the first
    //    travel leg, so it is an anchor before the next coordinate item.
    bool startAnchor = false;
    for (int j = firstCoordIndex + 1; j < count; j++) {
        SimpleMissionItem* it = qobject_cast<SimpleMissionItem*>(items->get(j));
        if (!it || (it->specifiesCoordinate() && it->coordinate().isValid())) {
            break;
        }
        if (it->command() == kCmdAuraAnchor) {
            startAnchor = true;
            break;
        }
    }

    // 4) Photo timing, from the FIRST stop. Every stop is written with the same numbers,
    //    so one is enough and the first one cannot be the closing waypoint of a return
    //    leg. Anchored: the surface anchor carries the window (param1) and the
    //    pre-shutter wait (param7) verbatim. Unanchored: only the CONDITION_DELAY is
    //    recoverable - see the header for why the window is not.
    int photoBefore = -1;
    int photoWindow = -1;
    for (int k = 1; k < count; k++) {
        SimpleMissionItem* wp = qobject_cast<SimpleMissionItem*>(items->get(k));
        if (!wp || !isSurfaceWaypoint(wp)) {
            continue;
        }
        for (int j = k + 1; j < count; j++) {
            SimpleMissionItem* it = qobject_cast<SimpleMissionItem*>(items->get(j));
            if (!it) {
                break;
            }
            const int cmd = it->command();
            if (cmd == kCmdAuraAnchor) {
                photoWindow = static_cast<int>(std::lround(it->missionItem().param1()));
                photoBefore = static_cast<int>(std::lround(it->missionItem().param7()));
                break;
            }
            if (cmd == kCmdConditionDelay) {
                photoBefore = static_cast<int>(std::lround(it->missionItem().param1()));
                continue;
            }
            if (cmd == kCmdConditionYaw || cmd == kCmdDoDigicamControl) {
                continue;
            }
            break;
        }
        break;
    }

    // 5) Return leg: the plan closes with a surface waypoint back on the start point.
    //    Compared against extractStart(), not plannedHomePosition - see extractStart()
    //    for why the Launch marker cannot be trusted here.
    bool returnHome = false;
    const QGeoCoordinate start = extractStart();
    for (int k = count - 1; k >= 1; k--) {
        SimpleMissionItem* it = qobject_cast<SimpleMissionItem*>(items->get(k));
        if (!it || !it->specifiesCoordinate() || !it->coordinate().isValid()) {
            continue;
        }
        returnHome = isSurfaceWaypoint(it) && start.isValid() && it->coordinate().distanceTo(start) < 1.0;
        break;
    }

    out[QStringLiteral("valid")]           = true;
    out[QStringLiteral("autoDepth")]       = autoDepth;
    out[QStringLiteral("cruiseDepth")]     = autoDepth ? kDefaultCruiseDepth : diveAlt;
    out[QStringLiteral("bottomClearance")] = autoDepth ? diveAlt : kDefaultBottomClearance;
    out[QStringLiteral("diveSettle")]      = diveSettle;
    out[QStringLiteral("startWait")]       = startWait;
    out[QStringLiteral("startAnchor")]     = startAnchor;
    out[QStringLiteral("returnHome")]      = returnHome;
    out[QStringLiteral("photoBefore")]     = photoBefore;
    out[QStringLiteral("photoWindow")]     = photoWindow;
    return out;
}

QVariantList StarMissionOnePlanCreator::extractTargets() const
{
    QVariantList targets;

    MissionController* missionController = _planMasterController ? _planMasterController->missionController() : nullptr;
    QmlObjectListModel* items = missionController ? missionController->visualItems() : nullptr;
    if (!items) {
        return targets;
    }

    // Walk the expanded pattern back to the targets that produced it. Index 0 is the
    // mission settings item; after that every stop is a surface waypoint followed by
    // the items that spell it out:
    //     surface WP -> anchor(31010)                                   anchored stop
    //     surface WP -> [CONDITION_YAW] CONDITION_DELAY DO_DIGICAM, WP  unanchored stop
    // An anchored stop also has a FIRST anchor, at cruise depth, ahead of its surface
    // waypoint - that is where the turn happens. It carries the same heading, so the
    // scan below reads everything it needs from the surface anchor and the deep one is
    // simply travel as far as this walk is concerned (it is a command item, never a
    // surface waypoint, so it cannot be mistaken for a stop of its own).
    // Everything else is travel: cruise legs sit at the cruise depth and a dive in
    // place just repeats the previous coordinate, so neither can be mistaken for a
    // stop. A plan with the return leg closes with a surface waypoint back at the
    // start; that one ends the mission and is not a target.
    const QGeoCoordinate start = missionController->plannedHomePosition();
    const int count = items->count();
    for (int i = 1; i < count; i++) {
        SimpleMissionItem* wp = qobject_cast<SimpleMissionItem*>(items->get(i));
        if (!wp || !isSurfaceWaypoint(wp)) {
            continue;
        }
        // Being last is not enough to call it the mission end. With the return leg
        // switched off the plan stops ON the final target, and a target that is neither
        // anchored nor photographed is a bare surface waypoint - identical in shape to
        // the closing one. What actually separates them is where they are: only the
        // closing waypoint sits on the start point.
        if (i == count - 1 && start.isValid() && wp->coordinate().distanceTo(start) < 1.0) {
            break;
        }

        bool   anchor = false;
        bool   camera = false;
        double yaw    = kNoYaw;

        for (int j = i + 1; j < count; j++) {
            SimpleMissionItem* it = qobject_cast<SimpleMissionItem*>(items->get(j));
            if (!it) {
                break;
            }
            const int cmd = it->command();
            if (cmd == kCmdAuraAnchor) {
                // param5 heading (negative = no turn), param6 shutter - the same
                // convention mavlink_int_to_mission_cmd() reads on the vehicle.
                anchor = true;
                const double heading = anchorField(it, it->missionItem().param5());
                yaw    = heading >= 0.0 ? heading : double(kNoYaw);
                camera = !qFuzzyIsNull(anchorField(it, it->missionItem().param6()));
                i = j;
                break;
            }
            if (cmd == kCmdConditionYaw) {
                yaw = it->missionItem().param1();
                continue;
            }
            if (cmd == kCmdDoDigicamControl) {
                camera = true;
                continue;
            }
            if (cmd == kCmdConditionDelay) {
                continue;
            }
            // The unanchored path parks a second surface waypoint on the same spot to
            // hold the photo window. Swallow it so it does not come back as a duplicate
            // target one metre from the first.
            if (isSurfaceWaypoint(it) && it->coordinate().distanceTo(wp->coordinate()) < 1.0) {
                i = j;
            }
            break;      // any other coordinate item is the dive out of this stop
        }

        QVariantMap target;
        target[QStringLiteral("coordinate")] = QVariant::fromValue(wp->coordinate());
        target[QStringLiteral("camera")]     = camera;
        target[QStringLiteral("yaw")]        = camera ? yaw : double(kNoYaw);
        target[QStringLiteral("anchor")]     = anchor;
        targets.append(target);
    }

    return targets;
}
