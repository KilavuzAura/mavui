/****************************************************************************
 *
 * (c) 2009-2019 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 *   @brief MavUI QGCCorePlugin Declaration
 *          Branding plugin for the AURA AUV ground control station (KilavuzAura team).
 */

#pragma once

#include <QtQml/QQmlAbstractUrlInterceptor>

#include "QGCCorePlugin.h"
#include "QGCOptions.h"

class CustomOptions;
class CustomPlugin;
class QQmlApplicationEngine;

Q_DECLARE_LOGGING_CATEGORY(CustomLog)

class CustomFlyViewOptions : public QGCFlyViewOptions
{
public:
    CustomFlyViewOptions(CustomOptions* options, QObject* parent = nullptr);

    // Overrides from QGCFlyViewOptions
    bool showMultiVehicleList(void) const final;
};

/*===========================================================================*/

class CustomOptions : public QGCOptions
{
public:
    CustomOptions(CustomPlugin *plugin, QObject* parent = nullptr);

    // Overrides from QGCOptions
    QGCFlyViewOptions* flyViewOptions(void) const final;

    // Use the platform-native file dialog everywhere. On Android this opens the
    // system document picker (SAF), letting files be chosen from the whole device
    // instead of QGC's app-folder-locked mobile dialog.
    bool useMobileFileDialog(void) const final { return false; }

private:
    CustomFlyViewOptions *_flyViewOptions = nullptr;
};

/*===========================================================================*/

class CustomPlugin : public QGCCorePlugin
{
    Q_OBJECT
public:
    explicit CustomPlugin(QObject *parent = nullptr);
    ~CustomPlugin();

    static QGCCorePlugin *instance();

    // Overrides from QGCCorePlugin
    void init() final;
    void cleanup() final;
    QGCOptions*             options                         (void) final;
    QString                 brandImageIndoor                (void) const final;
    QString                 brandImageOutdoor               (void) const final;
    bool                    overrideSettingsGroupVisibility (const QString &name) final;
    bool                    adjustSettingMetaData           (const QString& settingsGroup, FactMetaData& metaData) final;
    void                    paletteOverride                 (const QString &colorName, QGCPalette::PaletteColorInfo_t& colorInfo) final;
    QQmlApplicationEngine*  createQmlApplicationEngine      (QObject* parent) final;

private:
    CustomOptions*  _options = nullptr;
    QQmlApplicationEngine *_qmlEngine = nullptr;
    class CustomOverrideInterceptor *_selector = nullptr;
};

/*===========================================================================*/

class CustomOverrideInterceptor : public QQmlAbstractUrlInterceptor
{
public:
    CustomOverrideInterceptor();

    QUrl intercept(const QUrl &url, QQmlAbstractUrlInterceptor::DataType type) final;
};
