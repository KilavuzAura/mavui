/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "PlanCreator.h"

#include <QtCore/QVariantList>
#include <QtPositioning/QGeoCoordinate>

#include <limits>

/// Plan creator for the AURA "AUV Stars 2026 Mission One" competition task. The user
/// places a home position plus target waypoints (the square-area centers),
/// then the plan is expanded into the full underwater dive/surface/photo
/// pattern (mirrors tools/aura_foto_plan_uret.py). Interactive: the placement
/// happens in a dedicated mode before the plan is generated.
class StarMissionOnePlanCreator : public PlanCreator
{
    Q_OBJECT

public:
    StarMissionOnePlanCreator(PlanMasterController* planMasterController, QObject* parent = nullptr);

    static const QString name;

    // Interactive creators are driven from the placement mode, so the plain
    // click entry point does nothing on its own.
    Q_INVOKABLE void createPlan(const QGeoCoordinate& mapCenterCoord) final;

    /// Expands home + targets into the full mission and loads it into the plan.
    ///     home                planned home / start position (mission ends here at the surface)
    ///     targets             list of maps: { "coordinate": QGeoCoordinate,
    ///                                         "camera": bool,        // take a photo at this target
    ///                                         "yaw": double,         // camera heading in degrees, < 0 = no turn
    ///                                         "anchor": bool }       // drop anchor (MAV_CMD_AURA_ANCHOR) here
    ///     cruiseDepth         travel depth in meters, negative (0 / positive / NaN falls back to -1.0)
    ///     autoDepth           true: cruise legs ride the bottom (terrain frame, rangefinder) instead of
    ///                         a fixed barometric depth. Needs RNGFND1_* + WP_RFND_USE=1 on the vehicle.
    ///     surfaceClearance    minimum distance to keep below the surface while cruising (m, positive)
    ///     bottomClearance     distance to hold above the sea floor while cruising (m, positive)
    ///     photoBefore         CONDITION_DELAY before the shutter (s) — the "3" item in a generated plan.
    ///                         NaN falls back to StarMissionSettings::photoBefore.
    ///     photoWindow         hold on the photo-window waypoint (s) — the "5" item; a camera turn adds
    ///                         kTurnSeconds on top so the DO/CONDITION queue always fits inside it.
    ///                         NaN falls back to StarMissionSettings::photoWindow.
    ///     startAnchor         drop anchor once at the start point, right after the dive in place and
    ///                         before the first travel leg. Duration 0: the settle gate alone decides,
    ///                         so the mission departs from a point the vehicle has actually held.
    ///
    /// The anchor tuning and the dive/surface holds are not arguments: they come from
    /// StarMissionSettings (App Settings -> Mission One) on every call.
    Q_INVOKABLE void createFullPlan(const QGeoCoordinate&  home,
                                    const QVariantList&    targets,
                                    double                 cruiseDepth      = kDefaultCruiseDepth,
                                    bool                   autoDepth        = false,
                                    double                 surfaceClearance = kDefaultSurfaceClearance,
                                    double                 bottomClearance  = kDefaultBottomClearance,
                                    double                 photoBefore      = kUseSetting,
                                    double                 photoWindow      = kUseSetting,
                                    bool                   startAnchor      = false);

    /// Reads an already-loaded plan back into the target list that produced it, so the
    /// placement mode and the table can start from an existing plan instead of a blank
    /// sheet. Returns maps in exactly the shape createFullPlan() takes:
    ///     { "coordinate": QGeoCoordinate, "camera": bool, "yaw": double, "anchor": bool }
    /// Only the stops are recovered - the cruise depth, the holds and the anchor tuning
    /// are not read back, because they live in the UI boxes and StarMissionSettings.
    /// Returns an empty list when the loaded plan does not look like one of ours, which
    /// is the caller's signal that replacing it would lose work.
    Q_INVOKABLE QVariantList extractTargets() const;

    static constexpr double kDefaultCruiseDepth      = -1.0; ///< used when the UI leaves the depth box empty
    static constexpr double kDefaultSurfaceClearance =  0.3; ///< shallowest a cruise leg may be commanded (m below surface)
    static constexpr double kDefaultBottomClearance  =  1.0; ///< terrain-frame cruise altitude above the floor (m)
    /// Sentinel for "take this from StarMissionSettings" — the non-finite check in
    /// createFullPlan() already routes blank UI boxes down the same path.
    static constexpr double kUseSetting = std::numeric_limits<double>::quiet_NaN();
};
