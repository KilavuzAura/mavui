#ifndef CONNECTION_H
#define CONNECTION_H

#include <QObject>
#include <QSerialPort>
#include <QList>
#include "common/mavlink.h"
#include "missionmanager.h"

class Connection : public QObject {
    Q_OBJECT

public:
    explicit Connection(QObject *parent = nullptr);
    ~Connection();

    bool connectMavlink(const QString &portName, int baudRate);
    void disconnectMavlink();
    bool isConnected() const { return m_connected; }

    void uploadMission(const QList<Waypoint> &missionItems);
    void downloadMission();
    
    void sendRtcmData(const QByteArray &data);
    void setMissionCurrent(int seq);

signals:
    void connectionStatusChanged(bool connected);
    void mavlinkMessageReceived(const mavlink_message_t &msg);
    void missionUploadStatus(bool success, QString message);
    void missionDownloadFinished(const QList<Waypoint> &items);

private slots:
    void readPendingDatagrams();

private:
    QSerialPort *m_serial;
    bool m_connected;
    
    int m_targetSystem = 1;
    int m_targetComponent = 1;
    
    int m_downloadCount = 0;
    QList<Waypoint> m_downloadedItems;
    QList<Waypoint> m_uploadItems; 

    void sendMissionItem(int seq);
    void requestMissionItem(int seq);
};

#endif