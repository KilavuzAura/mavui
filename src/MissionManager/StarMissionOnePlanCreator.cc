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

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QVariantMap>
#include <QtCore/QTemporaryFile>
#include <QtCore/QDir>

const QString StarMissionOnePlanCreator::name = QStringLiteral("Star Mission One");

namespace {
// Movement pattern constants — kept in sync with tools/aura_foto_plan_uret.py.
constexpr double kCruiseDepth   = -1.0; // fixed cruise depth (m, negative = below surface)
// Surface waypoints are -0.1 m, NOT 0. Frame 3 altitudes are relative to HOME,
// and home is captured where the vehicle floats, so alt = 0 asks for a target at
// (or above) the waterline: the sub can never reach it, pins the vertical
// thrusters at full and hovers ~0.4 m down, taking the photo underwater. -0.1 m
// also keeps the depth sensor submerged (hull ~35 cm, sensor at mid height) while
// the camera on top clears the water. Matches aura_foto_plan_uret.py /
// gorev_yukle.py (SATIH_DERINLIK).
constexpr double kSurfaceDepth  = -0.1; // surface waypoint depth (m, negative)
// Hold on the dive-in-place waypoints. ArduSub declares a waypoint reached when
// the vehicle is within WPNAV_RADIUS in 3D, so a 1 m vertical leg completes while
// the sub is still ~0.5 m down (log: "Reached command #13" at 0.48 m with the
// target at 0.92 m) and the horizontal leg then runs shallow. The hold keeps the
// mission on the dive waypoint until the sub is actually at cruise depth.
constexpr int    kDiveSettle    = 5;    // seconds to settle after diving
constexpr int    kSurfaceSettle = 1;    // seconds to settle after surfacing
constexpr int    kPhotoBefore   = 3;    // seconds to wait before the photo
constexpr int    kPhotoAfter    = 2;    // seconds to wait after the photo
constexpr int    kTurnSeconds   = 2;    // fixed budget for the camera turn
constexpr int    kYawRate       = 90;   // camera yaw rate (deg/s)

constexpr int kCmdNavWaypoint       = 16;
constexpr int kCmdConditionDelay    = 112;
constexpr int kCmdConditionYaw      = 115;
constexpr int kCmdDoDigicamControl  = 203;
constexpr int kFrameGlobalRelativeAlt = 3;
constexpr int kFrameMission           = 2;

class PatternBuilder
{
public:
    // Cruise waypoint at depth (frame 3, negative altitude).
    void cruise(double lat, double lon, int hold = 0) {
        waypoint(lat, lon, kCruiseDepth, hold);
    }

    // Surface/absolute-depth waypoint (frame 3).
    void waypoint(double lat, double lon, double alt, int hold = 0) {
        QJsonObject o;
        o[QStringLiteral("AMSLAltAboveTerrain")] = QJsonValue::Null;
        o[QStringLiteral("Altitude")]            = alt;
        o[QStringLiteral("AltitudeMode")]        = 1;
        o[QStringLiteral("autoContinue")]        = true;
        o[QStringLiteral("command")]             = kCmdNavWaypoint;
        o[QStringLiteral("doJumpId")]            = _nextId();
        o[QStringLiteral("frame")]               = kFrameGlobalRelativeAlt;
        o[QStringLiteral("params")]              = QJsonArray({ hold, 0, 0, QJsonValue::Null, lat, lon, alt });
        o[QStringLiteral("type")]                = QStringLiteral("SimpleItem");
        _items.append(o);
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
    int _nextId() { return _jumpId++; }

    QJsonArray _items;
    int        _jumpId = 1;
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

void StarMissionOnePlanCreator::createFullPlan(const QGeoCoordinate& home, const QVariantList& targets)
{
    if (!home.isValid() || targets.isEmpty()) {
        return;
    }

    PatternBuilder b;
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

        b.cruise(prevLat, prevLon, kDiveSettle);    // 1) dive in place and settle at cruise depth
        b.cruise(lat, lon);                         // 2) travel to the target at depth
        b.waypoint(lat, lon, kSurfaceDepth, kSurfaceSettle);  // 3) surface and settle

        if (camera) {
            int window = kPhotoBefore + kPhotoAfter;
            if (yaw >= 0.0) {
                const int deg = (static_cast<int>(yaw) % 360 + 360) % 360;
                b.command(kCmdConditionYaw, QJsonArray({ deg, kYawRate, 0, 0, 0, 0, 0 })); // 4) turn to camera heading
                window += kTurnSeconds;
            }
            b.command(kCmdConditionDelay, QJsonArray({ kPhotoBefore, 0, 0, 0, 0, 0, 0 })); // 5) wait
            b.command(kCmdDoDigicamControl, QJsonArray({ 0, 0, 0, 0, 1, 0, 0 }));          // 6) take photo
            b.waypoint(lat, lon, kSurfaceDepth, window);                                          // 7) hold the photo window
        }

        prevLat = lat;
        prevLon = lon;
    }

    b.cruise(prevLat, prevLon, kDiveSettle);         // dive in place and settle at the last target
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
