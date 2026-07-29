/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "SettingsGroup.h"

/// Tuning for the AURA "AUV Stars 2026 Mission One" plan creator: the drop-anchor
/// command (MAV_CMD_AURA_ANCHOR / 31010) and the photo/dive timing around it.
/// Values persist through QSettings like every other SettingsGroup, so the panel
/// keeps whatever the operator entered across restarts.
class StarMissionSettings : public SettingsGroup
{
    Q_OBJECT
public:
    StarMissionSettings(QObject* parent = nullptr);
    DEFINE_SETTING_NAME_GROUP()

    DEFINE_SETTINGFACT(anchorRadius)
    DEFINE_SETTINGFACT(anchorSettle)
    DEFINE_SETTINGFACT(anchorGuard)
    DEFINE_SETTINGFACT(photoBefore)
    DEFINE_SETTINGFACT(photoWindow)
    DEFINE_SETTINGFACT(diveSettle)
    DEFINE_SETTINGFACT(surfaceSettle)

    /// Restores every fact in this group to the default from StarMission.SettingsGroup.json.
    Q_INVOKABLE void resetToDefaults();
};
