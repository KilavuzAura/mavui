/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "StarMissionSettings.h"

#include <QtQml/QQmlEngine>

DECLARE_SETTINGGROUP(StarMission, "StarMission")
{
    qmlRegisterUncreatableType<StarMissionSettings>("QGroundControl.SettingsManager", 1, 0, "StarMissionSettings", "Reference only");
}

DECLARE_SETTINGSFACT(StarMissionSettings, anchorRadius)
DECLARE_SETTINGSFACT(StarMissionSettings, anchorSettle)
DECLARE_SETTINGSFACT(StarMissionSettings, anchorGuard)
DECLARE_SETTINGSFACT(StarMissionSettings, photoBefore)
DECLARE_SETTINGSFACT(StarMissionSettings, photoWindow)
DECLARE_SETTINGSFACT(StarMissionSettings, startWait)
DECLARE_SETTINGSFACT(StarMissionSettings, diveSettle)
DECLARE_SETTINGSFACT(StarMissionSettings, surfaceSettle)

void StarMissionSettings::resetToDefaults()
{
    // Going through the accessors makes sure a fact that has not been touched yet
    // is created before it is reset, otherwise its stale QSettings entry survives.
    for (Fact* fact : { anchorRadius(), anchorSettle(), anchorGuard(),
                        photoBefore(), photoWindow(), startWait(),
                        diveSettle(), surfaceSettle() }) {
        if (fact->defaultValueAvailable()) {
            fact->setRawValue(fact->rawDefaultValue());
        }
    }
}
