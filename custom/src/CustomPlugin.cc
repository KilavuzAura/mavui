/****************************************************************************
 *
 * (c) 2009-2019 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 *   @brief MavUI QGCCorePlugin Implementation
 *          Branding plugin for the AURA AUV ground control station (KilavuzAura team).
 *          Vehicle: ArduSub on Orange Cube+.
 */

#include "CustomPlugin.h"
#include "QGCLoggingCategory.h"
#include "QGCPalette.h"
#include "QGCMAVLink.h"
#include "AppSettings.h"
#include "BrandImageSettings.h"
#include "FlightMapSettings.h"

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
#include <QtCore/QApplicationStatic>
#endif
#include <QtCore/QFile>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlFile>

QGC_LOGGING_CATEGORY(CustomLog, "gcs.custom.customplugin")

Q_APPLICATION_STATIC(CustomPlugin, _customPluginInstance);

CustomFlyViewOptions::CustomFlyViewOptions(CustomOptions* options, QObject* parent)
    : QGCFlyViewOptions(options, parent)
{

}

// MavUI drives a single AUV. Not supporting multiple vehicles simplifies various parts of the UI.
bool CustomFlyViewOptions::showMultiVehicleList(void) const
{
    return false;
}

CustomOptions::CustomOptions(CustomPlugin *plugin, QObject *parent)
    : QGCOptions(parent)
    , _flyViewOptions(new CustomFlyViewOptions(this, this))
{
    Q_UNUSED(plugin);
}

QGCFlyViewOptions* CustomOptions::flyViewOptions(void) const
{
    return _flyViewOptions;
}

/*===========================================================================*/

CustomPlugin::CustomPlugin(QObject *parent)
    : QGCCorePlugin(parent)
    , _options(new CustomOptions(this, this))
{

}

CustomPlugin::~CustomPlugin()
{

}

QGCCorePlugin *CustomPlugin::instance()
{
    return _customPluginInstance();
}

void CustomPlugin::init()
{

}

void CustomPlugin::cleanup()
{
    if (_qmlEngine) {
        _qmlEngine->removeUrlInterceptor(_selector);
    }

    delete _selector;
}

QGCOptions* CustomPlugin::options()
{
    return _options;
}

QString CustomPlugin::brandImageIndoor(void) const
{
    return QStringLiteral("/custom/img/AuraLogo.png");
}

QString CustomPlugin::brandImageOutdoor(void) const
{
    return QStringLiteral("/custom/img/AuraLogo.png");
}

bool CustomPlugin::overrideSettingsGroupVisibility(const QString &name)
{
    // We have set up our own specific brand imaging. Hide the brand image settings such that the end user
    // can't change it.
    if (name == BrandImageSettings::name) {
        return false;
    }
    return true;
}

// This allows you to override/hide QGC Application settings
bool CustomPlugin::adjustSettingMetaData(const QString& settingsGroup, FactMetaData& metaData)
{
    bool parentResult = QGCCorePlugin::adjustSettingMetaData(settingsGroup, metaData);

    if (settingsGroup == AppSettings::settingsGroup) {
        // When creating Plans while not connected to a vehicle, default to the
        // AURA vehicle: ArduSub (ArduPilot firmware, Sub vehicle class).
        if (metaData.name() == AppSettings::offlineEditingFirmwareClassName) {
            metaData.setRawDefaultValue(QGCMAVLink::FirmwareClassArduPilot);
            return false;
        } else if (metaData.name() == AppSettings::offlineEditingVehicleClassName) {
            metaData.setRawDefaultValue(QGCMAVLink::VehicleClassSub);
            return false;
        }
    } else if (settingsGroup == FlightMapSettings::settingsGroup) {
        // Stock default is Bing, whose legacy tile service now returns "no tile"
        // placeholders for every request — the flight map comes up empty.
        if (metaData.name() == FlightMapSettings::mapProviderName) {
            metaData.setRawDefaultValue(QStringLiteral("Esri"));
        } else if (metaData.name() == FlightMapSettings::mapTypeName) {
            metaData.setRawDefaultValue(QStringLiteral("World Satellite"));
        }
    }

    return parentResult;
}

// Retheme QGC to the Tokyo Night palette used by the team's previous GCS.
// The Dark color group carries the branding; the Light group keeps usable stock-like values.
void CustomPlugin::paletteOverride(const QString &colorName, QGCPalette::PaletteColorInfo_t& colorInfo)
{
    if (colorName == QStringLiteral("window")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#1a1b26");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#1a1b26");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#ffffff");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#f8f9fa");
    }
    else if (colorName == QStringLiteral("windowShade")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#24283b");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#24283b");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#f1f3f5");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#d9d9d9");
    }
    else if (colorName == QStringLiteral("windowShadeDark")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#16161e");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#16161e");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#e9ecef");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#bdbdbd");
    }
    else if (colorName == QStringLiteral("text")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#c0caf5");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#565f89");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#212529");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#9d9d9d");
    }
    else if (colorName == QStringLiteral("warningText")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#f7768e");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#f7768e");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#cc0808");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#cc0808");
    }
    else if (colorName == QStringLiteral("button")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#3b4261");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#3b4261");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#ffffff");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#ffffff");
    }
    else if (colorName == QStringLiteral("buttonText")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#c0caf5");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#565f89");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#212529");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#9d9d9d");
    }
    else if (colorName == QStringLiteral("buttonHighlight")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#7aa2f7");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#3b4261");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#aeebd0");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#e4e4e4");
    }
    else if (colorName == QStringLiteral("buttonHighlightText")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#1a1b26");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#565f89");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#212529");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#2c2c2c");
    }
    else if (colorName == QStringLiteral("primaryButton")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#7aa2f7");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#3b4261");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#aeebd0");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#585858");
    }
    else if (colorName == QStringLiteral("primaryButtonText")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#1a1b26");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#565f89");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#212529");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#cad0d0");
    }
    else if (colorName == QStringLiteral("textField")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#16161e");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#24283b");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#f1f3f5");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#ffffff");
    }
    else if (colorName == QStringLiteral("textFieldText")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#c0caf5");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#565f89");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#212529");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#808080");
    }
    else if (colorName == QStringLiteral("mapButton")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#16161e");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#565f89");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#212529");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#585858");
    }
    else if (colorName == QStringLiteral("mapButtonHighlight")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#7aa2f7");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#565f89");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#be781c");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#585858");
    }
    else if (colorName == QStringLiteral("mapIndicator")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#9ece6a");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#565f89");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#be781c");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#585858");
    }
    else if (colorName == QStringLiteral("mapIndicatorChild")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#73daca");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#565f89");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#766043");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#585858");
    }
    else if (colorName == QStringLiteral("colorGreen")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#9ece6a");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#9ece6a");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#009431");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#009431");
    }
    else if (colorName == QStringLiteral("colorOrange")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#e0af68");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#e0af68");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#b95604");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#b95604");
    }
    else if (colorName == QStringLiteral("colorRed")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#f7768e");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#f7768e");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#ed3939");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#ed3939");
    }
    else if (colorName == QStringLiteral("colorGrey")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#565f89");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#565f89");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#808080");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#808080");
    }
    else if (colorName == QStringLiteral("colorBlue")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#7aa2f7");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#7aa2f7");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#1a72ff");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#1a72ff");
    }
    else if (colorName == QStringLiteral("alertBackground")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#e0af68");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#e0af68");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#fffb8f");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#b45d48");
    }
    else if (colorName == QStringLiteral("alertBorder")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#a8763a");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#a8763a");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#808080");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#808080");
    }
    else if (colorName == QStringLiteral("alertText")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#1a1b26");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#1a1b26");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#212529");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#fff9ed");
    }
    else if (colorName == QStringLiteral("missionItemEditor")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#24283b");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#16161e");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#ffffff");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#585858");
    }
    else if (colorName == QStringLiteral("toolStripHoverColor")) {
        // This branch has no generic "hoverColor" key; toolStripHoverColor is the hover highlight.
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#7aa2f7");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#565f89");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#9d9d9d");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#585858");
    }
    else if (colorName == QStringLiteral("mapWidgetBorderLight")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#c0caf5");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#c0caf5");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#f1f3f5");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#ffffff");
    }
    else if (colorName == QStringLiteral("mapWidgetBorderDark")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#16161e");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#16161e");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#212529");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#000000");
    }
    else if (colorName == QStringLiteral("brandingPurple")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#bb9af7");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#bb9af7");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#4a2c6d");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#4a2c6d");
    }
    else if (colorName == QStringLiteral("brandingBlue")) {
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupEnabled]   = QColor("#7aa2f7");
        colorInfo[QGCPalette::Dark][QGCPalette::ColorGroupDisabled]  = QColor("#7aa2f7");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupEnabled]  = QColor("#6045c5");
        colorInfo[QGCPalette::Light][QGCPalette::ColorGroupDisabled] = QColor("#48d6ff");
    }
}

// We override this so we can install the qrc URL interceptor which redirects
// stock qrc paths to :/Custom overrides when present. This enables future QML
// overrides by simply adding files under the /Custom qrc prefix.
QQmlApplicationEngine* CustomPlugin::createQmlApplicationEngine(QObject* parent)
{
    _qmlEngine = QGCCorePlugin::createQmlApplicationEngine(parent);

    _selector = new CustomOverrideInterceptor();
    _qmlEngine->addUrlInterceptor(_selector);

    return _qmlEngine;
}

/*===========================================================================*/

CustomOverrideInterceptor::CustomOverrideInterceptor()
    : QQmlAbstractUrlInterceptor()
{

}

QUrl CustomOverrideInterceptor::intercept(const QUrl &url, QQmlAbstractUrlInterceptor::DataType type)
{
    switch (type) {
    using DataType = QQmlAbstractUrlInterceptor::DataType;
    case DataType::QmlFile:
    case DataType::UrlString:
        if (url.scheme() == QStringLiteral("qrc")) {
            const QString origPath = url.path();
            const QString overrideRes = QStringLiteral(":/Custom%1").arg(origPath);
            if (QFile::exists(overrideRes)) {
                const QString relPath = overrideRes.mid(2);
                QUrl result;
                result.setScheme(QStringLiteral("qrc"));
                result.setPath('/' + relPath);
                return result;
            }
        }
        break;
    default:
        break;
    }

    return url;
}
