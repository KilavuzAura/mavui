#include "sensorpub.h"
#include <QDebug>
#include <cmath>

SensorPub::SensorPub(QObject *parent) : QObject(parent) {
}

SensorData SensorPub::getData() const {
    return m_data;
}

void SensorPub::updateData(const mavlink_message_t &msg) {
    switch (msg.msgid) {
        case MAVLINK_MSG_ID_HEARTBEAT: processHeartbeat(msg); break;
        case MAVLINK_MSG_ID_SYS_STATUS: processSysStatus(msg); break;
        case MAVLINK_MSG_ID_GLOBAL_POSITION_INT: processGlobalPosition(msg); break;
        case MAVLINK_MSG_ID_ATTITUDE: processAttitude(msg); break;
        case MAVLINK_MSG_ID_VFR_HUD: processVfrHud(msg); break;
        case MAVLINK_MSG_ID_GPS_RAW_INT: processGpsRawInt(msg); break;
        case MAVLINK_MSG_ID_GPS_INPUT: processGpsInput(msg); break;
        case MAVLINK_MSG_ID_STATUSTEXT: processStatustext(msg); break;
        case MAVLINK_MSG_ID_SCALED_IMU: processScaledImu(msg); break;
        case MAVLINK_MSG_ID_NAV_CONTROLLER_OUTPUT: processNavControllerOutput(msg); break;
        case MAVLINK_MSG_ID_PID_TUNING: processPidTuning(msg); break;
        default: return;
    }
    emit dataUpdated(m_data);
}

void SensorPub::processHeartbeat(const mavlink_message_t &msg) {
    mavlink_heartbeat_t hb;
    mavlink_msg_heartbeat_decode(&msg, &hb);
    m_data.mode = hb.custom_mode;
    m_data.armed = (hb.base_mode & MAV_MODE_FLAG_SAFETY_ARMED);
}

void SensorPub::processSysStatus(const mavlink_message_t &msg) {
    mavlink_sys_status_t sys;
    mavlink_msg_sys_status_decode(&msg, &sys);
    m_data.batteryVoltage = sys.voltage_battery / 1000.0f;
    m_data.batteryCurrent = sys.current_battery / 100.0f;
    m_data.batteryRemaining = sys.battery_remaining;
}

void SensorPub::processGlobalPosition(const mavlink_message_t &msg) {
    mavlink_global_position_int_t pos;
    mavlink_msg_global_position_int_decode(&msg, &pos);
    m_data.lat = pos.lat / 1e7;
    m_data.lon = pos.lon / 1e7;
    m_data.alt = pos.relative_alt / 1000.0;
    m_data.heading = pos.hdg / 100.0f;
    m_data.vehicleAltMsl = pos.alt / 1000.0;
}

void SensorPub::processAttitude(const mavlink_message_t &msg) {
    mavlink_attitude_t att;
    mavlink_msg_attitude_decode(&msg, &att);
    m_data.roll = att.roll;
    m_data.pitch = att.pitch;
    m_data.yaw = att.yaw;
}

void SensorPub::processVfrHud(const mavlink_message_t &msg) {
    mavlink_vfr_hud_t vfr;
    mavlink_msg_vfr_hud_decode(&msg, &vfr);
    m_data.groundSpeed = vfr.groundspeed;
    m_data.airSpeed = vfr.airspeed;
    m_data.climbRate = vfr.climb;
}

void SensorPub::processGpsRawInt(const mavlink_message_t &msg) {
    mavlink_gps_raw_int_t gps;
    mavlink_msg_gps_raw_int_decode(&msg, &gps);
    m_data.fix_type = gps.fix_type;
    m_data.sat_visible = gps.satellites_visible;
    m_data.h_acc = gps.h_acc / 1000.0f; 
    m_data.v_acc = gps.v_acc / 1000.0f;
    m_data.gps_cog = gps.cog / 100.0f;
    m_data.gps_hdg_acc = gps.hdg_acc / 1e5f;
    m_data.gps_eph = gps.eph / 100.0f;
    m_data.gps_epv = gps.epv / 100.0f;

    if (gps.fix_type >= 3 && !m_data.rtkHomeSet) {
        m_data.rtkHomeLat = gps.lat / 1e7;
        m_data.rtkHomeLon = gps.lon / 1e7;
        m_data.homeAlt = gps.alt / 1000.0;
        m_data.rtkHomeSet = true;
    }
}

void SensorPub::processGpsInput(const mavlink_message_t &msg) {
    mavlink_gps_input_t gpsi;
    mavlink_msg_gps_input_decode(&msg, &gpsi);
    m_data.gps_input_time_week_ms = gpsi.time_week_ms;
    m_data.gps_input_vdop = gpsi.vdop;
}

void SensorPub::processStatustext(const mavlink_message_t &msg) {
    mavlink_statustext_t stat;
    mavlink_msg_statustext_decode(&msg, &stat);
    char text[51];
    memcpy(text, stat.text, 50);
    text[50] = '\0';
    m_data.statusText = QString(text);
}

void SensorPub::processScaledImu(const mavlink_message_t &msg) {
    mavlink_scaled_imu_t imu;
    mavlink_msg_scaled_imu_decode(&msg, &imu);
    m_data.mag_x = imu.xmag;
    m_data.mag_y = imu.ymag;
    m_data.mag_z = imu.zmag;
}

void SensorPub::processNavControllerOutput(const mavlink_message_t &msg) {
    mavlink_nav_controller_output_t nav;
    mavlink_msg_nav_controller_output_decode(&msg, &nav);
    m_data.nav_roll = nav.nav_roll;
    m_data.nav_pitch = nav.nav_pitch;
    m_data.nav_bearing = nav.nav_bearing;
    m_data.xtrack_error = nav.xtrack_error;
}

void SensorPub::processPidTuning(const mavlink_message_t &msg) {
    mavlink_pid_tuning_t pid;
    mavlink_msg_pid_tuning_decode(&msg, &pid);
    m_data.pidP = pid.P;
    m_data.pidI = pid.I;
    m_data.pidD = pid.D;
    m_data.pidAxis = pid.axis;
}