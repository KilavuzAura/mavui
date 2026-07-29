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
constexpr int    kTurnSeconds   = 2;    // fixed budget for the camera turn
constexpr int    kYawRate       = 90;   // camera yaw rate (deg/s)

// The dive/surface holds and the "drop anchor" (MAV_CMD_AURA_ANCHOR) tuning are
// operator-editable in App Settings -> Mission One (StarMissionSettings), so they
// are read per plan instead of being compile-time constants. Both matter because
// AC_WPNav latches reached_destination (AC_WPNav.cpp:540): once the sub touches
// WPNAV_RADIUS the hold timer runs even if drift then carries it away, and a
// vertical leg completes while the sub is still shallow.
struct MissionTuning {
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
                                               double                 photoWindow)
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

        b.cruise(prevLat, prevLon, tuning.diveSettle);  // 1) dive in place and settle at cruise depth
        b.cruise(lat, lon);                             // 2) travel to the target at depth
        b.waypoint(lat, lon, kSurfaceDepth, tuning.surfaceSettle);  // 3) surface and settle

        if (camera) {
            int window = photoWindowS;
            int queue  = photoBeforeS;
            if (yaw >= 0.0) {
                const int deg = (static_cast<int>(yaw) % 360 + 360) % 360;
                b.command(kCmdConditionYaw, QJsonArray({ deg, kYawRate, 0, 0, 0, 0, 0 })); // 4) turn to camera heading
                window += kTurnSeconds;
                queue  += kTurnSeconds;
            }
            // The window must outlast the DO/CONDITION queue: AP_Mission drops the
            // pending queue when the next nav command completes, so a short window
            // means the shutter never fires (advance_current_nav_cmd).
            window = std::max(window, queue + 1);

            b.command(kCmdConditionDelay, QJsonArray({ photoBeforeS, 0, 0, 0, 0, 0, 0 })); // 5) wait
            b.command(kCmdDoDigicamControl, QJsonArray({ 0, 0, 0, 0, 1, 0, 0 }));          // 6) take photo
            if (anchor) {
                // 7) Anchor instead of a plain hold: the queue still runs during it,
                //    but the mission waits until the sub has actually settled, so the
                //    photo is taken on-station.
                b.command(kCmdAuraAnchor, QJsonArray({ window, tuning.anchorRadius,
                                                       tuning.anchorSettle, tuning.anchorGuard, 0, 0, 0 }));
            } else {
                b.waypoint(lat, lon, kSurfaceDepth, window);                               // 7) hold the photo window
            }
        } else if (anchor) {
            // No photo here, but the operator still wants the sub pinned to the spot.
            b.command(kCmdAuraAnchor, QJsonArray({ photoWindowS, tuning.anchorRadius,
                                                   tuning.anchorSettle, tuning.anchorGuard, 0, 0, 0 }));
        }

        prevLat = lat;
        prevLon = lon;
    }

    b.cruise(prevLat, prevLon, tuning.diveSettle);   // dive in place and settle at the last target
    b.cruise(home.latitude(), home.longitude());     // return to home underwater
    b.waypoint(home.latitude(), home.longitude(), kSurfaceDepth); // surface, mission complete

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
