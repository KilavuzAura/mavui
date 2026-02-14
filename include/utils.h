#ifndef UTILS_H
#define UTILS_H

#include <QString>
#include <QApplication>

class Utils {
public:
    static QString getConfigPath();

    static QString getLogPath();
    
    static void loadStyleSheet(QApplication &app, const QString &filename);
    
    static void initFileSystem();
};

#endif // UTILS_H