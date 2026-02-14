#include "connection.h"
#include "logger.h"
#include <QDebug>
#include <QThread>
#include <cmath> 

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Connection::Connection(QObject *parent) : QObject(parent), m_serial(new QSerialPort(this)), m_connected(false) {
    connect(m_serial, &QSerialPort::readyRead, this, &Connection::readPendingDatagrams);
}

Connection::~Connection() {
    disconnectMavlink();
}

bool Connection::connectMavlink(const QString &portName, int baudRate) {
    if (m_connected) disconnectMavlink();
    
    m_serial->setPortName(portName);
    m_serial->setBaudRate(baudRate);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);
    
if (m_serial->open(QIODevice::ReadWrite)) {
    m_serial->setDataTerminalReady(true);
    m_serial->setRequestToSend(true);

    m_connected = true;
    emit connectionStatusChanged(true);
    Logger::log(Logger::Info, "Baglanti kuruldu: " + portName);
    return true;
}
    
    Logger::log(Logger::Error, QString("Baglanti hatasi (%1): %2").arg(portName).arg(m_serial->errorString()));
    return false;
}

void Connection::disconnectMavlink() {
    if (m_serial->isOpen()) m_serial->close();
    m_connected = false;
    emit connectionStatusChanged(false);
    Logger::log(Logger::Info, "Baglanti kesildi.");
}

void Connection::sendRtcmData(const QByteArray &data) {
    if(!m_connected || data.isEmpty()) return;

    const int maxLen = 180;
    int len = data.size();
    int offset = 0;
    uint8_t flags = 0; 

    while (offset < len) {
        int chunkSize = qMin(len - offset, maxLen);
        if ((offset + chunkSize) < len) flags |= 1; 
        else flags &= ~1;

        mavlink_message_t msg;
        uint8_t buf[180];
        memcpy(buf, data.constData() + offset, chunkSize);

        mavlink_msg_gps_rtcm_data_pack(255, 0, &msg, flags, chunkSize, buf);

        uint8_t sendBuf[MAVLINK_MAX_PACKET_LEN];
        uint16_t sendLen = mavlink_msg_to_send_buffer(sendBuf, &msg);
        m_serial->write((char*)sendBuf, sendLen);

        offset += chunkSize;
    }
}

void Connection::readPendingDatagrams() {
    QByteArray data = m_serial->readAll();
    mavlink_message_t msg;
    mavlink_status_t status;
    
    for (char c : data) {
        if (mavlink_parse_char(MAVLINK_COMM_0, static_cast<uint8_t>(c), &msg, &status)) {
            emit mavlinkMessageReceived(msg);
            
            if (msg.msgid == MAVLINK_MSG_ID_HEARTBEAT) {
                m_targetSystem = msg.sysid;
                m_targetComponent = msg.compid;
            }
            else if (msg.msgid == MAVLINK_MSG_ID_MISSION_REQUEST) {
                mavlink_mission_request_t req;
                mavlink_msg_mission_request_decode(&msg, &req);
                sendMissionItem(req.seq);
            }
            else if (msg.msgid == MAVLINK_MSG_ID_MISSION_REQUEST_INT) {
                mavlink_mission_request_int_t req;
                mavlink_msg_mission_request_int_decode(&msg, &req);
                sendMissionItem(req.seq);
            }
            else if (msg.msgid == MAVLINK_MSG_ID_MISSION_ACK) {
                mavlink_mission_ack_t ack;
                mavlink_msg_mission_ack_decode(&msg, &ack);
                if (ack.type == MAV_MISSION_ACCEPTED) 
                    emit missionUploadStatus(true, "Gorev basariyla yuklendi!");
                else 
                    emit missionUploadStatus(false, QString("Gorev yukleme hatasi: %1").arg(ack.type));
            }
            else if (msg.msgid == MAVLINK_MSG_ID_MISSION_COUNT) {
                mavlink_mission_count_t cnt;
                mavlink_msg_mission_count_decode(&msg, &cnt);
                m_downloadCount = cnt.count;
                m_downloadedItems.clear();
                if(m_downloadCount > 0) requestMissionItem(0);
                else emit missionDownloadFinished(m_downloadedItems);
            }
            else if (msg.msgid == MAVLINK_MSG_ID_MISSION_ITEM_INT) {
                mavlink_mission_item_int_t item;
                mavlink_msg_mission_item_int_decode(&msg, &item);
                Waypoint wp;
                wp.id = item.seq;
                wp.lat = item.x / 1e7;
                wp.lon = item.y / 1e7;
                wp.alt = item.z;
                m_downloadedItems.append(wp);
                
                if (m_downloadedItems.size() < m_downloadCount) {
                    requestMissionItem(m_downloadedItems.size());
                } else {
                    emit missionDownloadFinished(m_downloadedItems);
                    
                    mavlink_message_t ackMsg;
                    mavlink_msg_mission_ack_pack(255, 0, &ackMsg, m_targetSystem, m_targetComponent, MAV_MISSION_ACCEPTED, MAV_MISSION_TYPE_MISSION, 0);
                    
                    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
                    uint16_t len = mavlink_msg_to_send_buffer(buf, &ackMsg);
                    m_serial->write((char*)buf, len);
                }
            }
        }
    }
}

void Connection::uploadMission(const QList<Waypoint> &missionItems) {
    if(!m_connected) {
        emit missionUploadStatus(false, "Baglanti yok!");
        return;
    }
    
    m_uploadItems = missionItems; 
    
    mavlink_message_t msg;
    mavlink_msg_mission_count_pack(255, 0, &msg, m_targetSystem, m_targetComponent, m_uploadItems.size(), MAV_MISSION_TYPE_MISSION, 0);
    
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    m_serial->write((char*)buf, len);
}

void Connection::downloadMission() {
    if(!m_connected) return;
    mavlink_message_t msg;
    mavlink_msg_mission_request_list_pack(255, 0, &msg, m_targetSystem, m_targetComponent, MAV_MISSION_TYPE_MISSION);
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    m_serial->write((char*)buf, len);
}

void Connection::sendMissionItem(int seq) {
    if (seq < 0 || seq >= m_uploadItems.size()) return;
    
    const Waypoint &wp = m_uploadItems[seq];
    mavlink_message_t msg;
    
    mavlink_msg_mission_item_int_pack(255, 0, &msg,
                                      m_targetSystem, m_targetComponent,
                                      seq,
                                      MAV_FRAME_GLOBAL_RELATIVE_ALT,
                                      MAV_CMD_NAV_WAYPOINT,
                                      (seq == 0) ? 1 : 0, 
                                      1, 
                                      0, 
                                      (float)wp.radius, 
                                      0, 
                                      0, 
                                      (int32_t)(wp.lat * 1e7),
                                      (int32_t)(wp.lon * 1e7),
                                      (float)wp.alt,
                                      MAV_MISSION_TYPE_MISSION);
                                      
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    m_serial->write((char*)buf, len);
}

void Connection::requestMissionItem(int seq) {
    mavlink_message_t msg;
    mavlink_msg_mission_request_int_pack(255, 0, &msg, m_targetSystem, m_targetComponent, seq, MAV_MISSION_TYPE_MISSION);
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    m_serial->write((char*)buf, len);
}

void Connection::setMissionCurrent(int seq) {
    if(!m_connected) return;
    mavlink_message_t msg;
    // MAV_CMD_DO_SET_MISSION_CURRENT (224)
    // Param1: Sequence number
    mavlink_msg_command_long_pack(255, 0, &msg, m_targetSystem, m_targetComponent, 
                                  MAV_CMD_DO_SET_MISSION_CURRENT, 0, seq, 0, 0, 0, 0, 0, 0);
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    m_serial->write((char*)buf, len);
}