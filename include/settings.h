#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QSettings>
#include <QString>

class Settings : public QObject {
    Q_OBJECT
public:
    explicit Settings(QObject *parent = nullptr);

    void load();
    void save();
    void exec();

    QString getDefaultHome() const;
    void setDefaultHome(const QString &val);

    QString getMapTheme() const;
    void setMapTheme(const QString &theme);

    int getLeftSidebarWidth() const;
    void setLeftSidebarWidth(int width);

    int getSidebarWidth() const;
    void setSidebarWidth(int width);

    int getBottomBarHeight() const;
    void setBottomBarHeight(int height);

    QString getCamViewTR() const;
    void setCamViewTR(const QString &coords);
    QString getCamViewBL() const;
    void setCamViewBL(const QString &coords);

    int getMavBaud() const;
    void setMavBaud(int baud);
    QString getMavPort() const;
    void setMavPort(const QString &port);

    int getRtkBaud() const;
    void setRtkBaud(int baud);
    QString getRtkPort() const;
    void setRtkPort(const QString &port);

signals:
    void settingsChanged();
    void clearFilesRequested();
    void importStringRequested(const QString &data);

private:
    QSettings *m_store;
    QString m_defaultHome;
    QString m_mapTheme;
    int m_leftSidebarWidth;
    int m_sidebarWidth;
    int m_bottomBarHeight;
    QString m_camViewTR;
    QString m_camViewBL;
    int m_mavBaud;
    QString m_mavPort;
    int m_rtkBaud;
    QString m_rtkPort;
};

#endif