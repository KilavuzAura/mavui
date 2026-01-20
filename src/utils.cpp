#include "utils.h"
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>

QString Utils::getConfigPath() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/mavui";
    QDir dir(path);
    if (!dir.exists()) dir.mkpath(".");
    return path;
}

QString Utils::getLogPath() {
    QString path = QCoreApplication::applicationDirPath() + "/.log";
    QDir dir(path);
    if (!dir.exists()) dir.mkpath(".");
    return path;
}

void Utils::loadStyleSheet(QApplication &app, const QString &filename) {
    QString fullPath = getConfigPath() + "/" + filename;
    QFile file(fullPath);
    
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream stream(&file);
        app.setStyleSheet(stream.readAll());
        file.close();
        qDebug() << "Tema yuklendi:" << fullPath;
    } else {
        qWarning() << "Tema dosyasi bulunamadi:" << fullPath;
        qWarning() << "Varsayilan tema kullanilamiyor.";
    }
}

void Utils::initFileSystem() {
    QString targetPath = getConfigPath();
    
    QString sourcePath = QCoreApplication::applicationDirPath() + "/config";
    QDir sourceDir(sourcePath);

    if (sourceDir.exists()) {
        QStringList files = sourceDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
        
        for (const QString &f : files) {
            QString destFilePath = targetPath + "/" + f;
            QFile dFile(destFilePath);
            
            if (!dFile.exists()) {
                if (QFile::copy(sourceDir.absoluteFilePath(f), destFilePath)) {
                    qDebug() << "[INIT] Eksik dosya tamamlandi:" << f;
                    QFile::setPermissions(destFilePath, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);
                } else {
                    qWarning() << "[ERR] Dosya kopyalanamadi:" << f;
                }
            }
        }
    } else {
        qDebug() << "Build icindeki 'config' klasoru bulunamadi (Gelistirici modu disinda normal olabilir):" << sourcePath;
    }
    
    getLogPath();
}   