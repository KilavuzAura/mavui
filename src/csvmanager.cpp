#include "csvmanager.h"
#include "logger.h"
#include <QDateTime>
#include <QDir>

CsvManager::CsvManager(QObject *parent) : QObject(parent), m_file(nullptr), m_stream(nullptr), m_isLogging(false) {
}

CsvManager::~CsvManager() {
    stopLogging();
}

void CsvManager::startLogging() {
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString filename = QString("%1/.log/session_%2.csv").arg(QDir::currentPath(), timestamp);
    
    m_file = new QFile(filename);
    if (m_file->open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_stream = new QTextStream(m_file);
        
        *m_stream << "Time,Lat,Lon,Alt,Eph,Epv,Fix,Cog,RTK_Health,RTK_Acc,GSpeed,Roll,Pitch,Yaw,Compass,PWM_R,PWM_L,Setpoint\n";
        
        m_isLogging = true;
        Logger::log(Logger::Features, "CSV Log basladi: " + filename);
    }
}

void CsvManager::stopLogging() {
    if (m_file) {
        m_file->close();
        delete m_stream;
        delete m_file;
        m_file = nullptr;
        m_stream = nullptr;
        m_isLogging = false;
    }
}

void CsvManager::updateState(const DroneState &state) {
    m_currentState = state;
    if (m_isLogging) {
        writeRow();
    }
}

void CsvManager::writeRow() {
    if (!m_stream) return;

    QString rtkHealth = m_currentState.rtk_available ? QString::number(m_currentState.rtk_health) : "N/A";
    QString rtkAcc = m_currentState.rtk_available ? QString::number(m_currentState.rtk_accuracy) : "N/A";

    *m_stream << QDateTime::currentMSecsSinceEpoch() / 1000.0 << ","
              << QString::number(m_currentState.lat, 'f', 7) << ","
              << QString::number(m_currentState.lon, 'f', 7) << ","
              << QString::number(m_currentState.alt, 'f', 2) << ","
              << QString::number(m_currentState.eph, 'f', 2) << ","
              << QString::number(m_currentState.epv, 'f', 2) << ","
              << m_currentState.fix_type << ","
              << QString::number(m_currentState.cog, 'f', 2) << ","
              << rtkHealth << ","
              << rtkAcc << ","
              << QString::number(m_currentState.groundspeed, 'f', 2) << ","
              << QString::number(m_currentState.roll, 'f', 4) << ","
              << QString::number(m_currentState.pitch, 'f', 4) << ","
              << QString::number(m_currentState.yaw, 'f', 4) << ","
              << QString::number(m_currentState.heading, 'f', 2) << ","
              << m_currentState.pwm_right << ","
              << m_currentState.pwm_left << ","
              << QString::number(m_currentState.setpoint_speed, 'f', 2) << "\n";
}