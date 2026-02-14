#include "missionmanager.h"
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

MissionManager::MissionManager(QObject *parent) : QObject(parent) {
    Waypoint home;
    home.id = 0;
    home.lat = 0;
    home.lon = 0;
    home.alt = 0;
    home.radius = 0;
    home.isHome = true;
    m_missionItems.append(home);
    
    loadDefaults();
}

void MissionManager::updateHomePosition(double lat, double lon, double alt) {
    if(m_missionItems.isEmpty()) {
        Waypoint home;
        home.id = 0;
        home.isHome = true;
        m_missionItems.append(home);
    }
    m_missionItems[0].lat = lat;
    m_missionItems[0].lon = lon;
    m_missionItems[0].alt = alt;
    emit missionUpdated();
}

void MissionManager::addTargetWaypoint(double lat, double lon, double alt) {
    Waypoint wp;
    int nextId = 1;
    if(!m_missionItems.isEmpty()) {
        nextId = m_missionItems.last().id + 1;
    }
    
    wp.id = nextId;
    wp.lat = lat;
    wp.lon = lon;
    wp.alt = alt;
    wp.radius = 0;
    wp.isHome = false;
    m_missionItems.append(wp);
    emit missionUpdated();
}

void MissionManager::updateWaypoint(int id, double lat, double lon, double alt, double radius) {
    for(int i = 0; i < m_missionItems.size(); ++i) {
        if(m_missionItems[i].id == id) {
            m_missionItems[i].lat = lat;
            m_missionItems[i].lon = lon;
            m_missionItems[i].alt = alt;
            if(radius >= 0) m_missionItems[i].radius = radius;
            
            emit missionUpdated();
            emit markersUpdated(); 
            break;
        }
    }
}

void MissionManager::deleteWaypoint(int id) {
    if(id == 0) return;

    for(int i = 0; i < m_missionItems.size(); ++i) {
        if(m_missionItems[i].id == id) {
            m_missionItems.removeAt(i);
            reindexWaypoints(); 
            emit missionUpdated();
            emit markersUpdated(); 
            break;
        }
    }
}

void MissionManager::deleteZone(int index) {
    if(index >= 0 && index < m_zones.size()) {
        m_zones.removeAt(index);
        emit markersUpdated(); 
    }
}

void MissionManager::reindexWaypoints() {
    for(int i = 1; i < m_missionItems.size(); ++i) {
        m_missionItems[i].id = i;
    }
}

void MissionManager::addCircleToWaypoint(int wpId, double radius) {
    for(int i = 0; i < m_missionItems.size(); ++i) {
        if(m_missionItems[i].id == wpId) {
            m_missionItems[i].radius = radius;
            emit markersUpdated(); 
            break;
        }
    }
}

void MissionManager::addZonePoint(double lat, double lon) {
    m_tempZonePoints.append(QPointF(lat, lon));
    emit markersUpdated();
}

void MissionManager::completeZone() {
    if (m_tempZonePoints.size() > 2) {
        Zone z;
        z.points = m_tempZonePoints;
        m_zones.append(z);
        m_tempZonePoints.clear();
        emit markersUpdated();
    }
}

QList<Waypoint> MissionManager::getMissionItems() const {
    return m_missionItems;
}

QList<Zone> MissionManager::getZones() const {
    return m_zones;
}

QList<QPointF> MissionManager::getCurrentZonePoints() const {
    return m_tempZonePoints;
}

QList<CircleMarker> MissionManager::getCircles() const {
    QList<CircleMarker> circles;
    for(const auto &wp : m_missionItems) {
        if(wp.radius > 0) {
            CircleMarker cm;
            cm.wpId = wp.id;
            cm.lat = wp.lat;
            cm.lon = wp.lon;
            cm.radius = wp.radius;
            circles.append(cm);
        }
    }
    return circles;
}

void MissionManager::clearAll() {
    if (m_missionItems.size() > 1) {
        m_missionItems.erase(m_missionItems.begin() + 1, m_missionItems.end());
    }
    
    if (m_missionItems.isEmpty()) {
        Waypoint home;
        home.id = 0;
        home.lat = 0;
        home.lon = 0;
        home.alt = 0;
        home.radius = 0;
        home.isHome = true;
        m_missionItems.append(home);
    }
    
    m_zones.clear();
    m_tempZonePoints.clear();
    emit missionUpdated();
    emit markersUpdated();
}

void MissionManager::clearTargets() {
    if (m_missionItems.size() > 1) {
        m_missionItems.erase(m_missionItems.begin() + 1, m_missionItems.end());
    }
    emit missionUpdated();
    emit markersUpdated();
}

void MissionManager::clearDefaults() {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QFile::remove(configDir + "/mission_defaults.json");
    clearAll();
}

void MissionManager::saveDefaults() {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configDir);
    saveMissionToFile(configDir + "/mission_defaults.json");
}

void MissionManager::loadDefaults() {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    loadMissionFromFile(configDir + "/mission_defaults.json");
}

void MissionManager::saveMissionToFile(const QString &filePath) {
    QJsonObject root;
    QJsonArray wpArray;
    for(const auto &wp : m_missionItems) {
        QJsonObject wpObj;
        wpObj["id"] = wp.id;
        wpObj["lat"] = wp.lat;
        wpObj["lon"] = wp.lon;
        wpObj["alt"] = wp.alt;
        wpObj["rad"] = wp.radius;
        wpArray.append(wpObj);
    }
    root["waypoints"] = wpArray;
    
    QJsonArray zoneArray;
    for(const auto &z : m_zones) {
        QJsonArray points;
        for(const auto &p : z.points) {
            QJsonObject pObj;
            pObj["lat"] = p.x();
            pObj["lon"] = p.y();
            points.append(pObj);
        }
        zoneArray.append(points);
    }
    root["zones"] = zoneArray;

    QFile file(filePath);
    if(file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson());
        file.close();
    }
}

void MissionManager::loadMissionFromFile(const QString &filePath) {
    QFile file(filePath);
    if(!file.open(QIODevice::ReadOnly)) return;
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject root = doc.object();
    
    m_missionItems.clear();
    
    QJsonArray wpArray = root["waypoints"].toArray();
    bool homeFound = false;
    
    for(const auto &val : wpArray) {
        QJsonObject o = val.toObject();
        Waypoint wp;
        wp.id = o["id"].toInt();
        wp.lat = o["lat"].toDouble();
        wp.lon = o["lon"].toDouble();
        wp.alt = o["alt"].toDouble();
        wp.radius = o["rad"].toDouble();
        
        if(wp.id == 0) {
            wp.isHome = true;
            homeFound = true;
        } else {
            wp.isHome = false;
        }
        m_missionItems.append(wp);
    }
    
    if(!homeFound) {
        Waypoint home; home.id=0; home.isHome=true; 
        m_missionItems.prepend(home);
        reindexWaypoints(); 
    }

    m_zones.clear();
    QJsonArray zoneArray = root["zones"].toArray();
    for(const auto &zVal : zoneArray) {
        QJsonArray pArray = zVal.toArray();
        Zone z;
        for(const auto &pVal : pArray) {
            QJsonObject pObj = pVal.toObject();
            z.points.append(QPointF(pObj["lat"].toDouble(), pObj["lon"].toDouble()));
        }
        m_zones.append(z);
    }
    
    emit missionUpdated();
    emit markersUpdated();
}

void MissionManager::parseAndAddMissionString(const QString &data) {
    QString raw = data;
    if(raw.startsWith("~")) raw = raw.mid(1);
    
    QStringList parts = raw.split('*');
    for(const QString &p : parts) {
        QStringList vals = p.split(',');
        if(vals.size() >= 2) {
            double lat = vals[0].toDouble();
            double lon = vals[1].toDouble();
            double alt = (vals.size() > 2) ? vals[2].toDouble() : 10.0;
            double rad = (vals.size() > 3) ? vals[3].toDouble() : 0.0;
            
            addTargetWaypoint(lat, lon, alt);
            if(rad > 0) {
                addCircleToWaypoint(m_missionItems.last().id, rad);
            }
        }
    }
}