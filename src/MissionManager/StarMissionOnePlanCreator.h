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

/// Plan creator for the AURA "Star Mission One" competition task. The user
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
    ///     home     planned home / start position (mission ends here at the surface)
    ///     targets  list of maps: { "coordinate": QGeoCoordinate,
    ///                              "camera": bool,        // take a photo at this target
    ///                              "yaw": double }        // camera heading in degrees, < 0 = no turn
    Q_INVOKABLE void createFullPlan(const QGeoCoordinate& home, const QVariantList& targets);
};
