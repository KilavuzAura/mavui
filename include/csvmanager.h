#ifndef CSVMANAGER_H
#define CSVMANAGER_H

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QElapsedTimer>
#include "common/mavlink.h"

struct DroneState {
    uint64_t timestamp = 0;
    double lat = 0.0;
    double lon = 0.0;
    double alt = 0.0;
    double eph = 0.0;
    double epv = 0.0;
    int fix_type = 0;
    double cog = 0.0;
    int rtk_health = 0;
    double rtk_accuracy = 0.0;
    double groundspeed = 0.0;
    float roll = 0.0;
    float pitch = 0.0;
    float yaw = 0.0;
    float heading = 0.0;
    int pwm_right = 0;
    int pwm_left = 0;
    float setpoint_speed = 0.0;
    bool rtk_available = false;
};

class CsvManager : public QObject {
    Q_OBJECT
public:
    explicit CsvManager(QObject *parent = nullptr);
    ~CsvManager();

    void startLogging();
    void stopLogging();
    void updateState(const DroneState &state);
    void writeRow();

private:
    QFile *m_file;
    QTextStream *m_stream;
    bool m_isLogging;
    DroneState m_currentState;
};

#endif // CSVMANAGER_H