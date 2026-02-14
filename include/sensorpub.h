#ifndef SENSORPUB_H
#define SENSORPUB_H

#include <QObject>
#include <QString>
#include "ardupilotmega/mavlink.h"

struct SensorData {
    int mode = 0;
    bool armed = false;
    double lat = 0;
    double lon = 0;
    double alt = 0;
    float heading = 0;
    float roll = 0;
    float pitch = 0;
    float yaw = 0;
    float batteryVoltage = 0;
    float batteryCurrent = 0;
    int batteryRemaining = 0;
    float groundSpeed = 0;
    float airSpeed = 0;
    float climbRate = 0;
    
    QString statusText = "Sistem Hazir";

    float h_acc = 0; 
    float v_acc = 0; 
    int sat_visible = 0;
    int fix_type = 0;
    
    uint32_t gps_input_time_week_ms = 0;
    float gps_input_vdop = 0;
    
    double rtkHomeLat = 0;
    double rtkHomeLon = 0;
    bool rtkHomeSet = false;

    // Altitude
    double homeAlt = 0;
    double vehicleAltMsl = 0;

    // GPS extras
    float gps_cog = 0;
    float gps_hdg_acc = 0;
    float gps_eph = 0;
    float gps_epv = 0;

    // Magnetic field (SCALED_IMU)
    int16_t mag_x = 0;
    int16_t mag_y = 0;
    int16_t mag_z = 0;

    // NAV controller (NAV_CONTROLLER_OUTPUT)
    float nav_roll = 0;
    float nav_pitch = 0;
    int16_t nav_bearing = 0;
    float xtrack_error = 0;

    // PID tuning (PID_TUNING)
    float pidP = 0;
    float pidI = 0;
    float pidD = 0;
    uint8_t pidAxis = 0;
};

class SensorPub : public QObject {
    Q_OBJECT
public:
    explicit SensorPub(QObject *parent = nullptr);
    SensorData getData() const;
    void updateData(const mavlink_message_t &msg);

signals:
    void dataUpdated(const SensorData &data);

private:
    SensorData m_data;
    void processHeartbeat(const mavlink_message_t &msg);
    void processSysStatus(const mavlink_message_t &msg);
    void processGlobalPosition(const mavlink_message_t &msg);
    void processAttitude(const mavlink_message_t &msg);
    void processVfrHud(const mavlink_message_t &msg);
    void processGpsRawInt(const mavlink_message_t &msg);
    void processGpsInput(const mavlink_message_t &msg);
    void processStatustext(const mavlink_message_t &msg);
    void processScaledImu(const mavlink_message_t &msg);
    void processNavControllerOutput(const mavlink_message_t &msg);
    void processPidTuning(const mavlink_message_t &msg);
};

#endif