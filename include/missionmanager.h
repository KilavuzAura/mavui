#ifndef MISSIONMANAGER_H
#define MISSIONMANAGER_H

#include <QObject>
#include <QList>
#include <QPointF>
#include <QString>

struct Waypoint {
    int id;
    double lat;
    double lon;
    double alt;
    double radius;
    bool isHome;
};

struct Zone {
    QList<QPointF> points;
};

struct CircleMarker {
    int wpId; 
    double lat;
    double lon;
    double radius;
};

class MissionManager : public QObject {
    Q_OBJECT
public:
    explicit MissionManager(QObject *parent = nullptr);

    void updateHomePosition(double lat, double lon, double alt);
    void addTargetWaypoint(double lat, double lon, double alt);
    void updateWaypoint(int id, double lat, double lon, double alt, double radius);
    void deleteWaypoint(int id);
    
    void deleteZone(int index);

    void parseAndAddMissionString(const QString &text);
    void completeZone();
    void addCircleToWaypoint(int wpId, double radius);
    void clearAll();
    void clearTargets(); 
    
    void saveDefaults();
    void clearDefaults();
    void saveMissionToFile(const QString &path);
    void loadMissionFromFile(const QString &path);

    void addZonePoint(double lat, double lon);
    
    QList<Waypoint> getMissionItems() const;
    QList<Zone> getZones() const;
    QList<QPointF> getCurrentZonePoints() const;
    QList<CircleMarker> getCircles() const;

signals:
    void missionUpdated();
    void markersUpdated();

private:
    QList<Waypoint> m_missionItems;
    QList<Zone> m_zones;
    QList<QPointF> m_tempZonePoints; 

    void reindexWaypoints();
    void loadDefaults();
};

#endif