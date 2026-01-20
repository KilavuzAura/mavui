#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <QObject>

class Logger {
public:
    enum LogLevel {
        Info,
        Warning,
        Error,
        Debug,
        Features
    };

    static void log(LogLevel level, const QString &message);

    static QString readLog(LogLevel level);
    static void clearLog(LogLevel level);

private:
    static QString getFilePath(LogLevel level);
};

#endif // LOGGER_H