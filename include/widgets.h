#ifndef WIDGETS_H
#define WIDGETS_H

#include <QWidget>
#include <QList>
#include <QVariant>
#include "missionmanager.h"

class QVBoxLayout;
class QWebEngineView;
class QLabel;

class PFDWidget : public QWidget {
    Q_OBJECT
public:
    explicit PFDWidget(QWidget *parent = nullptr);
    void setAttitude(float roll, float pitch);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    float m_roll;
    float m_pitch;
};

class CompassWidget : public QWidget {
    Q_OBJECT
public:
    explicit CompassWidget(QWidget *parent = nullptr);
    void setHeading(float heading);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    float m_heading;
};

class HUDOverlay : public QWidget {
    Q_OBJECT
public:
    explicit HUDOverlay(QWidget *parent = nullptr);
    PFDWidget *pfd;
    CompassWidget *compass;
protected:
    void paintEvent(QPaintEvent *event) override;
};

class SimplePlot : public QWidget {
    Q_OBJECT
public:
    explicit SimplePlot(QString title, QWidget *parent = nullptr);
    void addData(double value);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    QString m_title;
    QList<double> m_data;
    int m_maxDataPoints;
    double m_minY;
    double m_maxY;
};

class GraphWindow : public QWidget {
    Q_OBJECT
public:
    explicit GraphWindow(QWidget *parent = nullptr);
    SimplePlot* addPlot(const QString& title);
private:
    QWidget *m_contentWidget;
    QVBoxLayout *m_layout;
};

class MapWidget : public QWidget {
    Q_OBJECT
public:
    explicit MapWidget(const QString &initialTheme, QWidget *parent = nullptr);
    void updatePosition(double lat, double lon, double heading);
    void drawMissionPath(const QList<Waypoint> &waypoints);
    void drawMarkers(const QList<Zone> &zones, const QList<QPointF> &currentZone, const QList<CircleMarker> &circles);
    void setClickMode(bool enable);
    void setZoneMode(bool enable);
    void setDraggableVehicle(bool enable); // Yeni eklenen metod
    void setBounds(double trLat, double trLon, double blLat, double blLon);
    void setCenter(double lat, double lon);
    void addOverlayWidget(QWidget* w);
    void runScript(const QString &script);

signals:
    void waypointClicked(double lat, double lon);
    void zoneClicked(double lat, double lon);
    void themeChanged(QString themeName);
    void vehicleMoved(double lat, double lon); // Yeni eklenen sinyal

protected:
    void resizeEvent(QResizeEvent *e) override;

private slots:
    void onLoadFinished(bool ok);
    void onUrlChanged(const QUrl &url);

private:
#ifdef USE_WEBENGINE
    QWebEngineView *m_webView;
#else
    QLabel *m_placeholder;
#endif
    bool m_isLoaded;
    QStringList m_pendingScripts;
    QList<QWidget*> m_overlays;
};

#endif // WIDGETS_H
