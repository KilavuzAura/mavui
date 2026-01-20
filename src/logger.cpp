#include "logger.h"
#include "utils.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDebug>

QString Logger::getFilePath(LogLevel level) {
    QString fileName;
    switch (level) {
        case Info:     fileName = "info.log"; break;
        case Warning:  fileName = "warning.log"; break;
        case Error:    fileName = "error.log"; break;
        case Debug:    fileName = "debug.log"; break;
        case Features: fileName = "features.log"; break;
        default:       fileName = "app.log"; break;
    }
    return Utils::getLogPath() + "/" + fileName;
}

void Logger::log(LogLevel level, const QString &message) {
    QString filePath = getFilePath(level);
    QFile file(filePath);
    
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
        
        QString levelStr;
        switch(level) {
            case Info: levelStr = "[INFO]"; break;
            case Warning: levelStr = "[WARN]"; break;
            case Error: levelStr = "[ERR]"; break;
            case Debug: levelStr = "[DBG]"; break;
            case Features: levelStr = "[FEAT]"; break;
        }
        
        out << timestamp << " " << levelStr << " " << message << "\n";
        file.close();
    } else {
        qWarning() << "Log dosyasina yazilamadi:" << filePath;
    }
}

QString Logger::readLog(LogLevel level) {
    QString filePath = getFilePath(level);
    QFile file(filePath);
    
    if (!file.exists()) return "";
    
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        return in.readAll();
    }
    return "";
}

void Logger::clearLog(LogLevel level) {
    QString filePath = getFilePath(level);
    QFile file(filePath);
    
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.close();
    }
}