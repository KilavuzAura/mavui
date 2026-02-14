#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QDockWidget>
#include <QLabel>
#include <QPushButton>
#include <QWidget>
#include <QDebug>
#include <QCloseEvent>
#include <QTabWidget>
#include <QScrollArea>
#include <QFormLayout>
#include <QGridLayout> 
#include <QFileDialog>
#include <QLineEdit>
#include <QGroupBox>
#include <QTimer>
#include <QDateTime>
#include <QMessageBox>
#include <QListWidget> 
#include <QClipboard>
#include <QComboBox>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QStandardPaths>
#include <QShortcut>
#include <QSizePolicy>
#include <algorithm>
#include <QCheckBox>
#include <QSpinBox>
#include <clocale>

#include "utils.h"
#include "settings.h"
#include "connection.h"
#include "logger.h"
#include "logviewer.h"
#include "csvmanager.h"
#include "widgets.h"
#include "sensorpub.h"
#include "missionmanager.h"
#include "common/mavlink.h" 

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr) : QMainWindow(parent), m_isRtkConnected(false), m_hasCenteredVehicle(false), m_targetWpIndex(-1), m_lastDataTime(0) {
        m_settings = new Settings(this);
        m_connection = new Connection(this);
        m_logViewer = new LogViewer(this);
        m_csvManager = new CsvManager(this);
        m_sensorPub = new SensorPub(this);
        m_missionManager = new MissionManager(this);
        m_rtkSerial = new QSerialPort(this);
        
        m_currentData.lat = 0;
        m_currentData.lon = 0;
        
        setupUI();
        setupShortcuts();
        
        connect(m_connection, &Connection::connectionStatusChanged, this, &MainWindow::onConnectionStatusChanged);
        connect(m_connection, &Connection::mavlinkMessageReceived, this, &MainWindow::processMavlinkMessage);
        connect(m_sensorPub, &SensorPub::dataUpdated, this, &MainWindow::updateTelemetry);
        connect(m_settings, &Settings::settingsChanged, this, &MainWindow::applySettings);
        connect(m_settings, &Settings::clearFilesRequested, m_missionManager, &MissionManager::clearDefaults);
        connect(m_settings, &Settings::importStringRequested, m_missionManager, &MissionManager::parseAndAddMissionString);

        connect(m_missionManager, &MissionManager::missionUpdated, this, [this](){
            m_map->drawMissionPath(m_missionManager->getMissionItems());
            refreshLists(); 
        });

        connect(m_missionManager, &MissionManager::markersUpdated, this, [this](){
            m_map->drawMarkers(m_missionManager->getZones(), 
                                m_missionManager->getCurrentZonePoints(),
                                m_missionManager->getCircles());
            refreshLists(); 
        });

        connect(m_map, &MapWidget::themeChanged, m_settings, &Settings::setMapTheme);

        connect(m_map, &MapWidget::waypointClicked, this, [this](double lat, double lon){
            m_inputLat->setText(QString::number(lat, 'f', 8));
            m_inputLon->setText(QString::number(lon, 'f', 8));
            
            bool found = false;
            for(const auto& wp : m_missionManager->getMissionItems()) {
                if(qAbs(wp.lat - lat) < 1e-7 && qAbs(wp.lon - lon) < 1e-7 && qAbs(wp.alt - 0.0) < 1e-7) {
                    m_missionManager->updateWaypoint(wp.id, lat, lon, 0.0, wp.radius);
                    found = true;
                    break;
                }
            }
            
            if(!found)
                m_missionManager->addTargetWaypoint(lat, lon, 0.0);
        });
        
        connect(m_map, &MapWidget::zoneClicked, this, [this](double lat, double lon){
            m_inputLat->setText(QString::number(lat, 'f', 8));
            m_inputLon->setText(QString::number(lon, 'f', 8));
            
            for(const auto& p : m_missionManager->getCurrentZonePoints()) {
                if(qAbs(p.x() - lat) < 1e-7 && qAbs(p.y() - lon) < 1e-7) return;
            }
            
            m_missionManager->addZonePoint(lat, lon);
        });

        connect(m_connection, &Connection::missionUploadStatus, this, [this](bool success, QString msg){
            if(success) QMessageBox::information(this, "Basarili", msg);
            else QMessageBox::warning(this, "Hata", msg);
        });
        
        connect(m_connection, &Connection::missionDownloadFinished, this, [this](const QList<Waypoint> &items){
            if (items.isEmpty()) {
                QMessageBox::information(this, "Bilgi", "Aracta kayitli gorev bulunamadi.");
                return;
            }
            m_missionManager->clearTargets();
            for (const auto &wp : items) {
                if (wp.id == 0) continue;

                bool found = false;
                for (const auto &existing : m_missionManager->getMissionItems()) {
                    if (qAbs(existing.lat - wp.lat) < 1e-7 && 
                        qAbs(existing.lon - wp.lon) < 1e-7 && 
                        qAbs(existing.alt - wp.alt) < 1e-7) {
                        m_missionManager->updateWaypoint(existing.id, wp.lat, wp.lon, wp.alt, wp.radius);
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    m_missionManager->addTargetWaypoint(wp.lat, wp.lon, wp.alt);
                    if (wp.radius > 0) {
                        auto list = m_missionManager->getMissionItems();
                        if (!list.isEmpty()) m_missionManager->addCircleToWaypoint(list.last().id, wp.radius);
                    }
                }
            }
            QMessageBox::information(this, "Basarili", "Gorev indirildi.");
        });

        connect(m_rtkSerial, &QSerialPort::readyRead, this, [this](){
            if(m_isRtkConnected) {
                QByteArray data = m_rtkSerial->readAll();
                m_connection->sendRtcmData(data);
            }
        });
        


        QTimer::singleShot(0, this, &MainWindow::applySettings);

        m_smoothTimer = new QTimer(this);
        connect(m_smoothTimer, &QTimer::timeout, this, &MainWindow::updateMapSmooth);
        m_smoothTimer->start(33); // ~30 FPS akici hareket icin
    }

    ~MainWindow() {}

protected:
    void closeEvent(QCloseEvent *event) override {
        if (m_mainSplitter) {
            QList<int> sizes = m_mainSplitter->sizes();
            if (sizes.size() >= 3) {
                m_settings->setLeftSidebarWidth(sizes[0]);
                m_settings->setSidebarWidth(sizes[2]);
            }
        }
        if (m_topDock && m_topDock->isVisible()) {
            m_settings->setBottomBarHeight(m_topDock->height());
        }
        QMainWindow::closeEvent(event);
    }

    bool eventFilter(QObject *obj, QEvent *event) override {
        if (event->type() == QEvent::ContextMenu) {
            if (obj == m_map || (m_map && m_map->isAncestorOf(qobject_cast<QWidget*>(obj)))) {
                return true;
            }
        }
        return QMainWindow::eventFilter(obj, event);
    }

private:
    Settings *m_settings;
    Connection *m_connection;
    LogViewer *m_logViewer;
    CsvManager *m_csvManager;
    SensorPub *m_sensorPub;
    MissionManager *m_missionManager;
    QSerialPort *m_rtkSerial;
    
    QSplitter *m_mainSplitter;
    QDockWidget *m_topDock;
    MapWidget *m_map;
    HUDOverlay *m_hudOverlay;
    
    QTabWidget *m_connectionTabs;
    QComboBox *m_cmbMavPort;
    QComboBox *m_cmbMavBaud;
    QPushButton *m_btnMavConnect;
    QPushButton *m_btnMavRefresh;
    
    QComboBox *m_cmbRtkPort;
    QComboBox *m_cmbRtkBaud;
    QPushButton *m_btnRtkConnect;
    QPushButton *m_btnRtkRefresh;
    bool m_isRtkConnected;
    bool m_hasCenteredVehicle;

    
    GraphWindow *m_graphWindow;
    SimplePlot *m_plotRoll;
    SimplePlot *m_plotPitch;
    SimplePlot *m_plotSpeed;
    
    QLabel *m_lblLat, *m_lblLon, *m_lblAlt, *m_lblHdg, *m_lblSpeed, *m_lblBatt, *m_lblArm;
    QLabel *m_lblSat, *m_lblHAcc, *m_lblVAcc, *m_lblRtkHome, *m_lblGpsInput; 
    QLabel *m_lblRoll, *m_lblPitch, *m_lblLinkQuality, *m_lblDistance, *m_lblTargetYaw, *m_lblYawError;
    QLabel *m_lblHomeAlt, *m_lblVehicleAltMsl;
    QLabel *m_lblGpsCog, *m_lblGpsHdgAcc, *m_lblSatCount;
    QLabel *m_lblMagX, *m_lblMagY, *m_lblMagZ;
    QLabel *m_lblNavRoll, *m_lblNavPitch, *m_lblNavBearing, *m_lblXtrack;
    QLabel *m_lblPidP, *m_lblPidI, *m_lblPidD;
    QLabel *m_statusLabel;
    QLabel *m_alertLabel;
    
    QLineEdit *m_inputLat, *m_inputLon, *m_inputAlt, *m_inputRadius;
    QPushButton *m_btnClearInputs;
    
    QPushButton *m_btnSaveItem;
    QPushButton *m_btnAddWp;
    QPushButton *m_btnAddZonePoint;
    QPushButton *m_btnCompleteZone;
    
    QPushButton *m_btnPickMapZone;
    QPushButton *m_btnPickMapWp;
    QPushButton *m_btnSwapWp;
    
    QTabWidget *m_listTabs;
    QListWidget *m_listWaypoints;
    QListWidget *m_listZones;
    QPushButton *m_btnEditItem;
    QPushButton *m_btnDeleteItem;
    
    QPushButton *m_btnSaveDefaults;    
    QPushButton *m_btnUploadVehicle;
    QPushButton *m_btnDownloadVehicle;
    QPushButton *m_btnClearMission;    
    QPushButton *m_btnSaveFile;        
    QPushButton *m_btnLoadFile;        

    
    // Tweaks & Nav Controls

    QPushButton *m_btnSetCurrentWp;
    int m_targetWpIndex;
    
    QTimer *m_smoothTimer;
    SensorData m_currentData;
    qint64 m_lastDataTime;

    void setupUI() {
        setWindowTitle("AURA GCS - Ground Control Station");
        resize(1280, 800);

        QString widgetStyle = R"(
            QLineEdit { background-color: #1a1b26; color: #a9b1d6; border: 1px solid #565f89; border-radius: 4px; padding: 5px; }
            QLineEdit:focus { border: 1px solid #7aa2f7; }
            QComboBox { background-color: #1a1b26; color: #a9b1d6; border: 1px solid #565f89; border-radius: 4px; padding: 4px; }
            QListWidget { background-color: #16161e; color: #c0caf5; border: 1px solid #565f89; border-radius: 4px; padding: 5px; outline: none; }
            QListWidget::item { padding: 5px; border-bottom: 1px solid #292e42; }
            QListWidget::item:selected { background-color: #3b82f6; color: white; }
            QPushButton { background-color: #3b4261; color: white; border-radius: 4px; padding: 6px; font-weight: bold; }
            QPushButton:hover { background-color: #444b6a; }
            QPushButton:checked { background-color: #7aa2f7; color: #1a1b26; }
            QGroupBox { border: 1px solid #3b4261; border-radius: 6px; margin-top: 12px; font-weight: bold; color: #c0caf5; } QGroupBox::title { subcontrol-origin: margin; subcontrol-position: center top; padding: 0 3px; }
        )";
        this->setStyleSheet(widgetStyle);

        m_graphWindow = new GraphWindow();
        m_plotRoll = m_graphWindow->addPlot("Roll");
        m_plotPitch = m_graphWindow->addPlot("Pitch");
        m_plotSpeed = m_graphWindow->addPlot("Hiz (Ground Speed)");

        QWidget *central = new QWidget(this);
        setCentralWidget(central);
        QHBoxLayout *mainLayout = new QHBoxLayout(central);
        mainLayout->setContentsMargins(0,0,0,0); mainLayout->setSpacing(0);

        m_map = new MapWidget(m_settings->getMapTheme(), this);
        m_map->setContextMenuPolicy(Qt::NoContextMenu);
        m_map->installEventFilter(this);
        for(auto *child : m_map->findChildren<QWidget*>()) {
            child->setContextMenuPolicy(Qt::NoContextMenu);
            child->installEventFilter(this);
        }
        m_hudOverlay = new HUDOverlay(m_map);
        m_hudOverlay->setContextMenuPolicy(Qt::NoContextMenu);
        m_map->addOverlayWidget(m_hudOverlay);
        
        QWidget *leftContainer = new QWidget(this);
        QVBoxLayout *leftLayout = new QVBoxLayout(leftContainer);
        leftLayout->setContentsMargins(10,10,10,10);
        leftLayout->setSpacing(10);

        QGridLayout *inputGrid = new QGridLayout();
        inputGrid->setSpacing(8);
        m_inputLat = new QLineEdit(); m_inputLat->setPlaceholderText("Lat");
        m_inputLon = new QLineEdit(); m_inputLon->setPlaceholderText("Lon");
        m_inputAlt = new QLineEdit(); m_inputAlt->setPlaceholderText("Alt");
        m_inputRadius = new QLineEdit(); m_inputRadius->setPlaceholderText("Radius (m)");
        m_btnClearInputs = new QPushButton("Temizle");
        m_btnClearInputs->setStyleSheet("background-color: #f7768e; color: white;");

        inputGrid->addWidget(new QLabel("Lat:"), 0, 0); inputGrid->addWidget(m_inputLat, 0, 1);
        inputGrid->addWidget(new QLabel("Lon:"), 1, 0); inputGrid->addWidget(m_inputLon, 1, 1);
        inputGrid->addWidget(new QLabel("Alt:"), 2, 0); inputGrid->addWidget(m_inputAlt, 2, 1);
        inputGrid->addWidget(new QLabel("Radius:"), 3, 0); inputGrid->addWidget(m_inputRadius, 3, 1);
        inputGrid->addWidget(m_btnClearInputs, 4, 0, 1, 2);

        leftLayout->addLayout(inputGrid);

        QHBoxLayout *actionRow1 = new QHBoxLayout();
        m_btnSaveItem = new QPushButton("Kaydet");
        m_btnAddWp = new QPushButton("Waypoint Ekle");
        m_btnAddWp->setStyleSheet("background-color: #9ece6a; color: #15161e;");
        m_btnAddZonePoint = new QPushButton("Zone Ekle");
        m_btnAddZonePoint->setStyleSheet("background-color: #e0af68; color: #15161e;");
        actionRow1->addWidget(m_btnSaveItem, 1);
        actionRow1->addWidget(m_btnAddWp, 1);
        actionRow1->addWidget(m_btnAddZonePoint, 1);
        leftLayout->addLayout(actionRow1);

        QHBoxLayout *zoneRow = new QHBoxLayout();
        m_btnCompleteZone = new QPushButton("Zone'u Tamamla");
        m_btnPickMapZone = new QPushButton("Haritadan Zone");
        m_btnPickMapZone->setCheckable(true);
        zoneRow->addWidget(m_btnCompleteZone, 1);
        zoneRow->addWidget(m_btnPickMapZone, 1);
        leftLayout->addLayout(zoneRow);

        QHBoxLayout *actionRow3 = new QHBoxLayout();
        m_btnSwapWp = new QPushButton("Yer Degistir");
        m_btnPickMapWp = new QPushButton("Haritadan WP");
        m_btnPickMapWp->setCheckable(true);
        actionRow3->addWidget(m_btnSwapWp, 1);
        actionRow3->addWidget(m_btnPickMapWp, 1);
        leftLayout->addLayout(actionRow3);

        leftLayout->addSpacing(10);
        
        QHBoxLayout *listEditLayout = new QHBoxLayout();
        m_btnEditItem = new QPushButton("Duzenle");
        m_btnDeleteItem = new QPushButton("Sil");
        m_btnDeleteItem->setStyleSheet("background-color: #f7768e;");
        listEditLayout->addWidget(m_btnEditItem, 1);
        listEditLayout->addWidget(m_btnDeleteItem, 1);
        leftLayout->addLayout(listEditLayout);
        
        m_btnSetCurrentWp = new QPushButton("HEDEF WAYPOINT YAP");
        m_btnSetCurrentWp->setStyleSheet("background-color: #bb9af7; color: #1a1b26; font-weight: bold; margin-bottom: 5px;");
        m_btnSetCurrentWp->setToolTip("Listeden secili waypoint'i hedef olarak ayarlar");
        leftLayout->insertWidget(leftLayout->count(), m_btnSetCurrentWp); // Listenin hemen altina veya ustune

        m_listTabs = new QTabWidget();
        m_listTabs->setStyleSheet("QTabWidget::pane { border: 1px solid #565f89; }");
        m_listWaypoints = new QListWidget();
        m_listWaypoints->setSelectionMode(QAbstractItemView::ExtendedSelection);
        m_listZones = new QListWidget();
        m_listTabs->addTab(m_listWaypoints, "Waypoints");
        m_listTabs->addTab(m_listZones, "Zones");
        leftLayout->addWidget(m_listTabs);

        QGroupBox *grpOps = new QGroupBox("Islemler");
        grpOps->setStyleSheet("QGroupBox { border: 1px solid #3b4261; border-radius: 6px; margin-top: 14px; padding-top: 10px; font-weight: bold; color: #c0caf5; } QGroupBox::title { subcontrol-origin: margin; subcontrol-position: center top; padding: 0 3px; }");
        QVBoxLayout *opsLayout = new QVBoxLayout(grpOps);
        m_btnSaveDefaults = new QPushButton("Varsayilana Kaydet");
        m_btnSaveDefaults->setStyleSheet("background-color: #7aa2f7; color: #15161e;");
        opsLayout->addWidget(m_btnSaveDefaults);

        QHBoxLayout *vehBtns = new QHBoxLayout();
        m_btnUploadVehicle = new QPushButton("Araca Yukle");
        m_btnDownloadVehicle = new QPushButton("Aractan Indir");
        vehBtns->addWidget(m_btnUploadVehicle, 1); vehBtns->addWidget(m_btnDownloadVehicle, 1);
        opsLayout->addLayout(vehBtns);

        QHBoxLayout *fileBtns = new QHBoxLayout();
        m_btnSaveFile = new QPushButton("Dosyaya Kaydet");
        m_btnLoadFile = new QPushButton("Dosyadan Yukle");
        fileBtns->addWidget(m_btnSaveFile, 1); fileBtns->addWidget(m_btnLoadFile, 1);
        opsLayout->addLayout(fileBtns);
        
        m_btnClearMission = new QPushButton("Gorevi Temizle");
        opsLayout->addWidget(m_btnClearMission);
        leftLayout->addWidget(grpOps);

        QWidget *rightSidebar = new QWidget(this);
        QVBoxLayout *rightLayout = new QVBoxLayout(rightSidebar);
        rightLayout->setContentsMargins(10, 10, 10, 10);
        rightLayout->setSpacing(6);

        m_connectionTabs = new QTabWidget(this);
        m_connectionTabs->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        m_connectionTabs->setStyleSheet("QTabWidget::pane { border: 1px solid #565f89; border-radius: 6px; margin: 2px; }");

        QWidget *mavTab = new QWidget();
        QVBoxLayout *mavLayout = new QVBoxLayout(mavTab);
        mavLayout->setContentsMargins(8, 8, 8, 8);
        mavLayout->setSpacing(5);
        mavLayout->setAlignment(Qt::AlignTop); 
        
        m_cmbMavPort = new QComboBox();
        m_cmbMavBaud = new QComboBox();
        QList<int> rates = {9600, 19200, 38400, 57600, 115200};
        for(int r : rates) m_cmbMavBaud->addItem(QString::number(r));
        m_cmbMavBaud->setCurrentText("57600");
        m_btnMavRefresh = new QPushButton("Yenile");
        
        m_btnMavConnect = new QPushButton("BAGLAN");
        m_btnMavConnect->setStyleSheet("background-color: #9ece6a; color: white; font-weight: bold; padding: 6px;");
        
        QHBoxLayout *mavRow1 = new QHBoxLayout();
        mavRow1->addWidget(new QLabel("Port:")); mavRow1->addWidget(m_cmbMavPort, 1); mavRow1->addWidget(m_btnMavRefresh);
        mavLayout->addLayout(mavRow1);
        
        QHBoxLayout *mavRow2 = new QHBoxLayout();
        mavRow2->addWidget(new QLabel("Baud:")); mavRow2->addWidget(m_cmbMavBaud, 1);
        mavLayout->addLayout(mavRow2);
        
        mavLayout->addWidget(m_btnMavConnect);
        
        QWidget *rtkTab = new QWidget();
        QVBoxLayout *rtkLayout = new QVBoxLayout(rtkTab);
        rtkLayout->setContentsMargins(8, 8, 8, 8);
        rtkLayout->setSpacing(5);
        rtkLayout->setAlignment(Qt::AlignTop); 
        
        m_cmbRtkPort = new QComboBox();
        m_cmbRtkBaud = new QComboBox();
        for(int r : rates) m_cmbRtkBaud->addItem(QString::number(r));
        m_cmbRtkBaud->setCurrentText("57600");
        m_btnRtkRefresh = new QPushButton("Yenile");
        
        m_btnRtkConnect = new QPushButton("BAGLAN");
        m_btnRtkConnect->setStyleSheet("background-color: #9ece6a; color: white; font-weight: bold; padding: 6px;");
        
        QHBoxLayout *rtkRow1 = new QHBoxLayout();
        rtkRow1->addWidget(new QLabel("Port:")); rtkRow1->addWidget(m_cmbRtkPort, 1); rtkRow1->addWidget(m_btnRtkRefresh);
        rtkLayout->addLayout(rtkRow1);
        
        QHBoxLayout *rtkRow2 = new QHBoxLayout();
        rtkRow2->addWidget(new QLabel("Baud:")); rtkRow2->addWidget(m_cmbRtkBaud, 1);
        rtkLayout->addLayout(rtkRow2);
        
        rtkLayout->addWidget(m_btnRtkConnect);

        m_connectionTabs->addTab(mavTab, "MAVLink");
        m_connectionTabs->addTab(rtkTab, "RTK");
        rightLayout->addWidget(m_connectionTabs);

        QHBoxLayout *rightBtnLayout = new QHBoxLayout();
        rightBtnLayout->setContentsMargins(0, 5, 0, 5);
        QPushButton *btnSettings = new QPushButton("AYARLAR");
        QPushButton *btnLogs = new QPushButton("LOGLAR");
        rightBtnLayout->addWidget(btnSettings, 1);
        rightBtnLayout->addWidget(btnLogs, 1);
        rightLayout->addLayout(rightBtnLayout);

        auto createStatBox = [](QString title, QLabel* valueLbl) -> QFrame* {
            QFrame* frame = new QFrame();
            frame->setStyleSheet("background-color: #1f2335; border-radius: 6px; border: 1px solid #3b4261;");
            QVBoxLayout* l = new QVBoxLayout(frame);
            l->setContentsMargins(10, 8, 10, 8); l->setSpacing(2);
            QLabel* titleLbl = new QLabel(title);
            titleLbl->setStyleSheet("color: #787c99; font-size: 10px; font-weight: bold; border: none; background: transparent;");
            valueLbl->setStyleSheet("color: #7aa2f7; font-size: 14px; font-weight: bold; border: none; background: transparent;");
            valueLbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            l->addWidget(titleLbl); l->addWidget(valueLbl);
            frame->setFixedHeight(50);
            return frame;
        };

        m_lblLat = new QLabel("0.000000"); m_lblLon = new QLabel("0.000000"); 
        m_lblAlt = new QLabel("0.0 m"); m_lblHdg = new QLabel("0°");
        m_lblSpeed = new QLabel("0.0 m/s"); m_lblBatt = new QLabel("0.0 V");
        m_lblSat = new QLabel("0"); m_lblArm = new QLabel("DISARMED");
        m_lblHAcc = new QLabel("-"); m_lblVAcc = new QLabel("-");
        m_lblRtkHome = new QLabel("Yok"); m_lblGpsInput = new QLabel("-");
        m_lblRoll = new QLabel("0.0°"); m_lblPitch = new QLabel("0.0°"); m_lblLinkQuality = new QLabel("N/A");
        m_lblDistance = new QLabel("- m");
        m_lblTargetYaw = new QLabel("-°"); m_lblYawError = new QLabel("-°");
        m_lblHomeAlt = new QLabel("-"); m_lblVehicleAltMsl = new QLabel("-");
        m_lblGpsCog = new QLabel("-"); m_lblGpsHdgAcc = new QLabel("-"); m_lblSatCount = new QLabel("0");
        m_lblMagX = new QLabel("0"); m_lblMagY = new QLabel("0"); m_lblMagZ = new QLabel("0");
        m_lblNavRoll = new QLabel("-"); m_lblNavPitch = new QLabel("-"); m_lblNavBearing = new QLabel("-"); m_lblXtrack = new QLabel("-");
        m_lblPidP = new QLabel("0"); m_lblPidI = new QLabel("0"); m_lblPidD = new QLabel("0");

        QScrollArea *statsScroll = new QScrollArea();
        statsScroll->setWidgetResizable(true);
        statsScroll->setFrameShape(QFrame::NoFrame);
        statsScroll->setStyleSheet("QScrollArea { background: transparent; border: none; } QScrollBar:vertical { background: #1a1b26; width: 6px; } QScrollBar::handle:vertical { background: #3b4261; border-radius: 3px; } QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");
        QWidget *statsContainer = new QWidget();
        QVBoxLayout *statsLayout = new QVBoxLayout(statsContainer);
        statsLayout->setSpacing(10);
        statsLayout->setContentsMargins(0, 0, 0, 0);

        QGroupBox *grpPfd = new QGroupBox("ARAC VERILERI (PFD)");
        grpPfd->setStyleSheet("QGroupBox { border: 1px solid #3b4261; border-radius: 6px; margin-top: 14px; padding-top: 10px; font-weight: bold; color: #7aa2f7; } QGroupBox::title { subcontrol-origin: margin; subcontrol-position: center top; padding: 0 3px; }");
        QGridLayout *pfdGrid = new QGridLayout(grpPfd);
        pfdGrid->addWidget(createStatBox("ROLL", m_lblRoll), 0, 0); pfdGrid->addWidget(createStatBox("PITCH", m_lblPitch), 0, 1); pfdGrid->addWidget(createStatBox("YAW", m_lblHdg), 0, 2);
        pfdGrid->addWidget(createStatBox("HIZ", m_lblSpeed), 1, 0); pfdGrid->addWidget(createStatBox("IRTIF", m_lblAlt), 1, 1);
        pfdGrid->addWidget(createStatBox("IRTIF (MSL)", m_lblVehicleAltMsl), 1, 2);
        pfdGrid->addWidget(createStatBox("HEDEF YAW", m_lblTargetYaw), 2, 0);
        pfdGrid->addWidget(createStatBox("HATA YAW", m_lblYawError), 2, 1);
        pfdGrid->addWidget(createStatBox("HOME IRTIF", m_lblHomeAlt), 2, 2);
        statsLayout->addWidget(grpPfd);

        QGroupBox *grpSys = new QGroupBox("SISTEM DURUMU");
        grpSys->setStyleSheet("QGroupBox { border: 1px solid #3b4261; border-radius: 6px; margin-top: 14px; padding-top: 10px; font-weight: bold; color: #f7768e; } QGroupBox::title { subcontrol-origin: margin; subcontrol-position: center top; padding: 0 3px; }");
        QGridLayout *sysGrid = new QGridLayout(grpSys);
        sysGrid->addWidget(createStatBox("BATARYA", m_lblBatt), 0, 0);
        sysGrid->addWidget(createStatBox("LINK KALITE", m_lblLinkQuality), 0, 1);

        QFrame* armFrame = new QFrame();
        armFrame->setFixedHeight(50);
        armFrame->setStyleSheet("background-color: #1f2335; border-radius: 6px; border: 1px solid #3b4261;");
        QHBoxLayout* armLayout = new QHBoxLayout(armFrame);
        QLabel* armTitle = new QLabel("DURUM:");
        armTitle->setStyleSheet("color: #787c99; font-size: 12px; font-weight: bold; border: none;");
        m_lblArm->setStyleSheet("color: #9ece6a; font-size: 16px; font-weight: bold; border: none;");
        m_lblArm->setAlignment(Qt::AlignCenter);
        armLayout->addWidget(armTitle); armLayout->addStretch(); armLayout->addWidget(m_lblArm); armLayout->addStretch();
        
        sysGrid->addWidget(armFrame, 1, 0, 1, 2);
        statsLayout->addWidget(grpSys);

        QGroupBox *grpGps = new QGroupBox("GPS & KONUM");
        grpGps->setStyleSheet("QGroupBox { border: 1px solid #3b4261; border-radius: 6px; margin-top: 14px; padding-top: 10px; font-weight: bold; color: #9ece6a; } QGroupBox::title { subcontrol-origin: margin; subcontrol-position: center top; padding: 0 3px; }");
        QGridLayout *gpsGrid = new QGridLayout(grpGps);
        gpsGrid->addWidget(createStatBox("ENLEM", m_lblLat), 0, 0); gpsGrid->addWidget(createStatBox("BOYLAM", m_lblLon), 0, 1);
        gpsGrid->addWidget(createStatBox("HEDEF MESAFE", m_lblDistance), 1, 0); gpsGrid->addWidget(createStatBox("GPS INPUT", m_lblGpsInput), 1, 1);
        gpsGrid->addWidget(createStatBox("UYDU", m_lblSatCount), 2, 0); gpsGrid->addWidget(createStatBox("FIX TIPI", m_lblSat), 2, 1);
        gpsGrid->addWidget(createStatBox("YATAY HATA", m_lblHAcc), 3, 0); gpsGrid->addWidget(createStatBox("DIKEY HATA", m_lblVAcc), 3, 1);
        gpsGrid->addWidget(createStatBox("GPS YON", m_lblGpsCog), 4, 0); gpsGrid->addWidget(createStatBox("YON HATASI", m_lblGpsHdgAcc), 4, 1);
        gpsGrid->addWidget(createStatBox("RTK HOME", m_lblRtkHome), 5, 0, 1, 2);
        statsLayout->addWidget(grpGps);

        QGroupBox *grpMag = new QGroupBox("MAG FIELD");
        grpMag->setStyleSheet("QGroupBox { border: 1px solid #3b4261; border-radius: 6px; margin-top: 14px; padding-top: 10px; font-weight: bold; color: #bb9af7; } QGroupBox::title { subcontrol-origin: margin; subcontrol-position: center top; padding: 0 3px; }");
        QGridLayout *magGrid = new QGridLayout(grpMag);
        magGrid->addWidget(createStatBox("MAG X", m_lblMagX), 0, 0);
        magGrid->addWidget(createStatBox("MAG Y", m_lblMagY), 0, 1);
        magGrid->addWidget(createStatBox("MAG Z", m_lblMagZ), 0, 2);
        statsLayout->addWidget(grpMag);

        QGroupBox *grpNav = new QGroupBox("NAV / IMU");
        grpNav->setStyleSheet("QGroupBox { border: 1px solid #3b4261; border-radius: 6px; margin-top: 14px; padding-top: 10px; font-weight: bold; color: #ff9e64; } QGroupBox::title { subcontrol-origin: margin; subcontrol-position: center top; padding: 0 3px; }");
        QGridLayout *navGrid = new QGridLayout(grpNav);
        navGrid->addWidget(createStatBox("NAV ROLL", m_lblNavRoll), 0, 0);
        navGrid->addWidget(createStatBox("NAV PITCH", m_lblNavPitch), 0, 1);
        navGrid->addWidget(createStatBox("NAV BEARING", m_lblNavBearing), 1, 0);
        navGrid->addWidget(createStatBox("XTRACK", m_lblXtrack), 1, 1);
        statsLayout->addWidget(grpNav);

        QGroupBox *grpPid = new QGroupBox("KONTROL (PID)");
        grpPid->setStyleSheet("QGroupBox { border: 1px solid #3b4261; border-radius: 6px; margin-top: 14px; padding-top: 10px; font-weight: bold; color: #73daca; } QGroupBox::title { subcontrol-origin: margin; subcontrol-position: center top; padding: 0 3px; }");
        QGridLayout *pidGrid = new QGridLayout(grpPid);
        pidGrid->addWidget(createStatBox("PID P", m_lblPidP), 0, 0);
        pidGrid->addWidget(createStatBox("PID I", m_lblPidI), 0, 1);
        pidGrid->addWidget(createStatBox("PID D", m_lblPidD), 0, 2);
        statsLayout->addWidget(grpPid);

        statsLayout->addStretch();

        statsScroll->setWidget(statsContainer);
        rightLayout->addWidget(statsScroll, 1);
        
        QPushButton *btnOpenGraph = new QPushButton("GRAFIKLER");
        btnOpenGraph->setStyleSheet("background-color: #7aa2f7; color: #1a1b26; font-weight: bold; padding: 15px; margin-top: 10px;");
        connect(btnOpenGraph, &QPushButton::clicked, m_graphWindow, &GraphWindow::show);
        rightLayout->addWidget(btnOpenGraph);



        m_mainSplitter = new QSplitter(Qt::Horizontal, this);
        m_mainSplitter->addWidget(leftContainer);
        m_mainSplitter->addWidget(m_map);
        m_mainSplitter->addWidget(rightSidebar);
        
        m_mainSplitter->setStretchFactor(0, 0);
        m_mainSplitter->setStretchFactor(1, 1);
        m_mainSplitter->setStretchFactor(2, 0);
        
        m_mainSplitter->setCollapsible(0, true);
        m_mainSplitter->setCollapsible(2, true);
        mainLayout->addWidget(m_mainSplitter);

        m_topDock = new QDockWidget("Kontrol", this);
        m_topDock->setTitleBarWidget(new QWidget());
        QWidget *dockWidget = new QWidget();
        QHBoxLayout *dockLayout = new QHBoxLayout(dockWidget);
        dockLayout->setContentsMargins(10, 5, 10, 5); // Margin eklendi
        dockLayout->addStretch();
        QVBoxLayout *statusLayout = new QVBoxLayout();
        

        
        statusLayout->setContentsMargins(10, 10, 10, 10);
        m_statusLabel = new QLabel("BAGLANTI YOK");
        m_statusLabel->setStyleSheet("font-weight: bold; color: #f7768e; font-size: 14px; background: transparent;");
        m_alertLabel = new QLabel("Sistem Bekleniyor...");
        m_alertLabel->setStyleSheet("color: #565f89; font-weight: bold; font-size: 12px; background: transparent;");
        m_statusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_alertLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        statusLayout->addWidget(m_statusLabel); statusLayout->addWidget(m_alertLabel);
        dockLayout->addLayout(statusLayout);
        QLabel *lblBottomLogo = new QLabel();
        QPixmap logoPixmap(":/resources/icons/Aura.png");
        lblBottomLogo->setPixmap(logoPixmap.scaled(128, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        lblBottomLogo->setStyleSheet("margin-left: 15px; border-left: 1px solid #3b4261; padding-left: 10px; margin-right: 10px;");
        dockLayout->addWidget(lblBottomLogo);
        m_topDock->setWidget(dockWidget);
        addDockWidget(Qt::TopDockWidgetArea, m_topDock);

        connect(m_btnMavConnect, &QPushButton::clicked, this, &MainWindow::toggleConnection);
        connect(m_btnMavRefresh, &QPushButton::clicked, this, [this](){ refreshPorts(m_cmbMavPort); });
        connect(m_btnRtkConnect, &QPushButton::clicked, this, [this](){
            if(m_btnRtkConnect->text() == "BAGLAN") {
                m_rtkSerial->setPortName(m_cmbRtkPort->currentText());
                m_rtkSerial->setBaudRate(m_cmbRtkBaud->currentText().toInt());
                if(m_rtkSerial->open(QIODevice::ReadOnly)) {
                    m_btnRtkConnect->setText("KES");
                    m_btnRtkConnect->setStyleSheet("background-color: #f7768e; color: white; font-weight: bold; padding: 6px;");
                    m_isRtkConnected = true;
                } else QMessageBox::warning(this, "Hata", "RTK baglantisi acilamadi!");
            } else {
                m_rtkSerial->close();
                m_btnRtkConnect->setText("BAGLAN");
                m_btnRtkConnect->setStyleSheet("background-color: #9ece6a; color: white; font-weight: bold; padding: 6px;");
                m_isRtkConnected = false;
            }
        });
        connect(m_btnRtkRefresh, &QPushButton::clicked, this, [this](){ refreshPorts(m_cmbRtkPort); });
        connect(btnSettings, &QPushButton::clicked, m_settings, &Settings::exec);
        connect(btnLogs, &QPushButton::clicked, m_logViewer, &LogViewer::exec);
        
        connect(m_btnPickMapWp, &QPushButton::clicked, [this](bool checked){
            if(checked) {
                m_btnPickMapZone->setChecked(false);
            }
            m_map->setClickMode(checked);
        });
        connect(m_btnPickMapZone, &QPushButton::clicked, [this](bool checked){
            if(checked) {
                m_btnPickMapWp->setChecked(false);
            }
            m_map->setZoneMode(checked);
        });

        connect(m_btnClearInputs, &QPushButton::clicked, this, [this](){
            m_inputLat->clear(); m_inputLon->clear(); m_inputAlt->clear(); m_inputRadius->clear();
        });

        connect(m_btnAddWp, &QPushButton::clicked, this, [this](){
            bool okLat, okLon;
            double lat = m_inputLat->text().toDouble(&okLat);
            double lon = m_inputLon->text().toDouble(&okLon);
            
            if(!okLat || !okLon) {
                QMessageBox::warning(this, "Gecersiz Giris", "Lutfen gecerli bir Enlem ve Boylam giriniz.");
                return;
            }
            
            double alt = m_inputAlt->text().toDouble();
            double rad = m_inputRadius->text().toDouble();
            
            bool found = false;
            for(const auto& wp : m_missionManager->getMissionItems()) {
                if(qAbs(wp.lat - lat) < 1e-7 && qAbs(wp.lon - lon) < 1e-7 && qAbs(wp.alt - alt) < 1e-7) {
                    m_missionManager->updateWaypoint(wp.id, lat, lon, alt, rad);
                    found = true;
                    break;
                }
            }
            
            if(!found) {
                m_missionManager->addTargetWaypoint(lat, lon, alt);
                if(rad > 0) {
                    int lastId = m_missionManager->getMissionItems().last().id;
                    m_missionManager->addCircleToWaypoint(lastId, rad);
                }
            }
        });

        connect(m_btnAddZonePoint, &QPushButton::clicked, this, [this](){
            bool okLat, okLon;
            double lat = m_inputLat->text().toDouble(&okLat);
            double lon = m_inputLon->text().toDouble(&okLon);
            if(!okLat || !okLon) {
                QMessageBox::warning(this, "Gecersiz Giris", "Lutfen gecerli bir Enlem ve Boylam giriniz.");
                return;
            }
            for(const auto& p : m_missionManager->getCurrentZonePoints()) {
                if(qAbs(p.x() - lat) < 1e-7 && qAbs(p.y() - lon) < 1e-7) {
                    return;
                }
            }
            m_missionManager->addZonePoint(lat, lon);
        });
        connect(m_btnCompleteZone, &QPushButton::clicked, m_missionManager, &MissionManager::completeZone);
        
        connect(m_btnSaveDefaults, &QPushButton::clicked, m_missionManager, &MissionManager::saveDefaults);
        connect(m_btnClearMission, &QPushButton::clicked, m_missionManager, &MissionManager::clearAll);
        connect(m_btnUploadVehicle, &QPushButton::clicked, this, [this](){ m_connection->uploadMission(m_missionManager->getMissionItems()); });
        connect(m_btnDownloadVehicle, &QPushButton::clicked, m_connection, &Connection::downloadMission);
        connect(m_btnSaveFile, &QPushButton::clicked, this, [this](){
            QString path = QFileDialog::getSaveFileName(this, "Kaydet", "", "JSON (*.json)");
            if(!path.isEmpty()) {
                if (!path.endsWith(".json", Qt::CaseInsensitive)) {
                    path += ".json";
                }
                m_missionManager->saveMissionToFile(path);
            }
        });
        connect(m_btnLoadFile, &QPushButton::clicked, this, [this](){
            QString path = QFileDialog::getOpenFileName(this, "Yukle", "", "JSON (*.json)");
            if(!path.isEmpty()) m_missionManager->loadMissionFromFile(path);
        });

        connect(m_btnSwapWp, &QPushButton::clicked, this, [this](){
            if(m_listTabs->currentIndex() != 0) return;
            auto selected = m_listWaypoints->selectedItems();
            if(selected.size() != 2) {
                QMessageBox::warning(this, "Hata", "Yer degistirmek icin 2 waypoint secin (Ctrl ile).");
                return;
            }
            int r1 = m_listWaypoints->row(selected[0]);
            int r2 = m_listWaypoints->row(selected[1]);
            
            QList<Waypoint> items = m_missionManager->getMissionItems();
            if(r1 < 0 || r1 >= items.size() || r2 < 0 || r2 >= items.size()) return;
            
            if(items[r1].id == 0 || items[r2].id == 0) {
                QMessageBox::warning(this, "Hata", "Home noktasi tasinamaz.");
                return;
            }
            
            std::swap(items[r1], items[r2]);
            
            m_missionManager->clearTargets();
            for(const auto &wp : items) {
                if(wp.id != 0) {
                    m_missionManager->addTargetWaypoint(wp.lat, wp.lon, wp.alt);
                    if(wp.radius > 0) {
                        auto currentList = m_missionManager->getMissionItems();
                        if(!currentList.isEmpty()) m_missionManager->addCircleToWaypoint(currentList.last().id, wp.radius);
                    }
                }
            }
        });

        connect(m_btnSaveItem, &QPushButton::clicked, this, [this](){
            if(m_listTabs->currentIndex() == 0 && m_listWaypoints->currentItem()) {
                int id = m_listWaypoints->currentItem()->data(Qt::UserRole).toInt();
                bool okLat, okLon;
                double lat = m_inputLat->text().toDouble(&okLat);
                double lon = m_inputLon->text().toDouble(&okLon);
                double alt = m_inputAlt->text().toDouble();
                double rad = m_inputRadius->text().toDouble();
                
                if(okLat && okLon) {
                    m_missionManager->updateWaypoint(id, lat, lon, alt, rad);
                }
            }
        });

        connect(m_btnDeleteItem, &QPushButton::clicked, this, [this](){
            int tabIdx = m_listTabs->currentIndex();
            
            if(tabIdx == 0) { // Waypoints
                QList<QListWidgetItem*> selected = m_listWaypoints->selectedItems();
                for(auto item : selected) {
                    int id = item->data(Qt::UserRole).toInt();
                    if(id == 0) {
                        QMessageBox::warning(this, "Uyari", "Home (WP 0) silinemez.");
                        continue;
                    }
                    m_missionManager->deleteWaypoint(id);
                }
            }
            else if(tabIdx == 1) { // Zones
                QList<QListWidgetItem*> selected = m_listZones->selectedItems();
                if(selected.isEmpty()) return;
                
                QList<int> rows;
                for(auto* item : selected) {
                    rows.append(m_listZones->row(item));
                }
                std::sort(rows.begin(), rows.end(), std::greater<int>());
                
                for(int r : rows) {
                    m_missionManager->deleteZone(r);
                }
            }
        });

        connect(m_btnEditItem, &QPushButton::clicked, this, [this](){
            if(m_listTabs->currentIndex() == 0 && m_listWaypoints->currentItem()) {
                int id = m_listWaypoints->currentItem()->data(Qt::UserRole).toInt();
                for(const auto& wp : m_missionManager->getMissionItems()) {
                    if(wp.id == id) {
                        m_inputLat->setText(QString::number(wp.lat, 'f', 8));
                        m_inputLon->setText(QString::number(wp.lon, 'f', 8));
                        m_inputAlt->setText(QString::number(wp.alt));
                        if(wp.radius > 0) m_inputRadius->setText(QString::number(wp.radius));
                        else m_inputRadius->clear();
                        break;
                    }
                }
            }
        });

        refreshPorts(m_cmbMavPort); refreshPorts(m_cmbRtkPort);
        

        
        connect(m_btnSetCurrentWp, &QPushButton::clicked, this, [this](){
            if(m_listTabs->currentIndex() == 0 && m_listWaypoints->currentItem()) {
                int id = m_listWaypoints->currentItem()->data(Qt::UserRole).toInt();
                m_targetWpIndex = id;
                
                // Haritada goster
                for(const auto& wp : m_missionManager->getMissionItems()) {
                    if(wp.id == id) {
                        m_map->runScript(QString("drawDesiredWaypoint(%1, %2);").arg(wp.lat, 0, 'f', 8).arg(wp.lon, 0, 'f', 8));
                        break;
                    }
                }
                
                // Araca komut gonder
                m_connection->setMissionCurrent(id);
                QMessageBox::information(this, "Bilgi", QString("Hedef Waypoint %1 olarak ayarlandi.").arg(id));
            } else {
                QMessageBox::warning(this, "Hata", "Lutfen listeden bir waypoint secin.");
            }
        });
        
    }
    
    void setupShortcuts() {
        QShortcut *scGraph = new QShortcut(QKeySequence("Ctrl+G"), this);
        scGraph->setContext(Qt::ApplicationShortcut);
        connect(scGraph, &QShortcut::activated, this, [this](){
            if(m_graphWindow->isVisible()) m_graphWindow->hide();
            else m_graphWindow->show();
        });

        QShortcut *scHome = new QShortcut(QKeySequence("Ctrl+H"), this);
        scHome->setContext(Qt::ApplicationShortcut);
        connect(scHome, &QShortcut::activated, this, [this](){
            auto items = m_missionManager->getMissionItems();
            if(!items.isEmpty() && items[0].isHome && (items[0].lat != 0.0 && items[0].lon != 0.0)) {
                m_map->setCenter(items[0].lat, items[0].lon);
            } else {
                QMessageBox::warning(this, "Konum Yok", "Home konumu henüz ayarlanmadi.");
            }
        });

        QShortcut *scVehicle = new QShortcut(QKeySequence("Ctrl+V"), this);
        scVehicle->setContext(Qt::ApplicationShortcut);
        connect(scVehicle, &QShortcut::activated, this, [this](){
            SensorData data = m_sensorPub->getData();
            if(data.lat != 0.0 && data.lon != 0.0) {
                m_map->setCenter(data.lat, data.lon);
            } else {
                QMessageBox::warning(this, "Konum Yok", "Aracin konumu henüz bilinmiyor.");
            }
        });
        

    }

    void refreshPorts(QComboBox *box) {
        box->clear();
        QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
        if(ports.isEmpty()) {
            box->addItem("(Port Yok)");
        } else {
            for(const QSerialPortInfo &info : ports) box->addItem(info.portName());
        }
    }

    void applySettings() {
        if(m_settings->getLeftSidebarWidth() > 0 && m_mainSplitter->count() >= 3) {
            QList<int> sizes;
            sizes << m_settings->getLeftSidebarWidth();
            int mid = width() - m_settings->getLeftSidebarWidth() - m_settings->getSidebarWidth();
            if(mid < 10) mid = 10;
            sizes << mid;
            sizes << m_settings->getSidebarWidth();
            m_mainSplitter->setSizes(sizes);
        }
        if(m_settings->getBottomBarHeight() > 0) resizeDocks({m_topDock}, {m_settings->getBottomBarHeight()}, Qt::Vertical);
        
        m_map->runScript(QString("if(typeof setMapTheme === 'function') setMapTheme('%1');").arg(m_settings->getMapTheme()));
        
        m_map->runScript("setMapCenter(39.0, 35.0, 6);");

        if (!m_settings->getMavPort().isEmpty()) {
            int idx = m_cmbMavPort->findText(m_settings->getMavPort());
            if (idx != -1) m_cmbMavPort->setCurrentIndex(idx);
        }
        m_cmbMavBaud->setCurrentText(QString::number(m_settings->getMavBaud()));

        if (!m_settings->getRtkPort().isEmpty()) {
            int idx = m_cmbRtkPort->findText(m_settings->getRtkPort());
            if (idx != -1) m_cmbRtkPort->setCurrentIndex(idx);
        }
        m_cmbRtkBaud->setCurrentText(QString::number(m_settings->getRtkBaud()));
    }
    
    void refreshLists() {
        int selectedId = -1;
        if(m_listWaypoints->currentItem()) {
            selectedId = m_listWaypoints->currentItem()->data(Qt::UserRole).toInt();
        }

        m_listWaypoints->clear();
        for(const auto& wp : m_missionManager->getMissionItems()) {
            QString prefix = (wp.id == 0) ? "HOME" : QString("WP %1").arg(wp.id);
            QString txt = QString("%1: %2, %3 (%4m)").arg(prefix).arg(wp.lat, 0, 'f', 6).arg(wp.lon, 0, 'f', 6).arg(wp.alt);
            if(wp.radius > 0) txt += QString(" [R: %1m]").arg(wp.radius);
            QListWidgetItem *item = new QListWidgetItem(txt);
            item->setData(Qt::UserRole, wp.id);
            if(wp.id == 0) item->setForeground(Qt::green);
            m_listWaypoints->addItem(item);
            
            if(wp.id == selectedId) {
                m_listWaypoints->setCurrentItem(item);
            }
        }
        m_listZones->clear();
        int zIdx = 1;
        for(const auto& zone : m_missionManager->getZones()) {
            m_listZones->addItem(QString("Zone %1 (%2 Points)").arg(zIdx++).arg(zone.points.size()));
        }
    }

    void toggleConnection() {
        if (m_connection->isConnected()) {
            m_connection->disconnectMavlink();
            m_btnMavConnect->setText("BAGLAN");
            m_btnMavConnect->setStyleSheet("background-color: #9ece6a; color: white; font-weight: bold; padding: 6px;");
            m_statusLabel->setText("BAGLANTI KESILDI");
            m_statusLabel->setStyleSheet("font-weight: bold; color: #f7768e; font-size: 14px; background: transparent;");
        } else {
            QString portName = m_cmbMavPort->currentText();
            if(portName == "(Port Yok)") return;
            int baudRate = m_cmbMavBaud->currentText().toInt();
            if (m_connection->connectMavlink(portName, baudRate)) {
                m_btnMavConnect->setText("KES");
                m_btnMavConnect->setStyleSheet("background-color: #f7768e; color: white; font-weight: bold; padding: 6px;");
                m_statusLabel->setText("BAGLANDI (" + portName + ")");
                m_statusLabel->setStyleSheet("font-weight: bold; color: #9ece6a; font-size: 14px; background: transparent;");
            } else {
                QMessageBox::critical(this, "Hata", "Baglanti acilamadi!");
            }
        }
    }
    
    void onConnectionStatusChanged(bool connected) {
        if (!connected && m_btnMavConnect->text() == "KES") {
            m_btnMavConnect->setText("BAGLAN");
            m_btnMavConnect->setStyleSheet("background-color: #9ece6a; color: white; font-weight: bold; padding: 6px;");
            m_statusLabel->setText("BAGLANTI KOPTU");
            m_statusLabel->setStyleSheet("font-weight: bold; color: #f7768e; font-size: 14px; background: transparent;");
            
            m_hasCenteredVehicle = false;
            // Baglanti kopunca RTK Home varsa oraya don
            SensorData data = m_sensorPub->getData();
            if(data.rtkHomeSet) {
                m_map->setCenter(data.rtkHomeLat, data.rtkHomeLon);
            }
        } else if (connected) {
            m_hasCenteredVehicle = false;
        }
    }

    void processMavlinkMessage(const mavlink_message_t &msg) {
        if (m_sensorPub) m_sensorPub->updateData(msg);

        if (msg.msgid == MAVLINK_MSG_ID_BATTERY_STATUS) {
            mavlink_battery_status_t batt;
            mavlink_msg_battery_status_decode(&msg, &batt);
            if (batt.voltages[0] != UINT16_MAX) {
                double voltage = batt.voltages[0] / 1000.0; // mV -> V
                m_lblBatt->setText(QString::number(voltage, 'f', 1) + " V");
            }
        }
        
        if (msg.msgid == MAVLINK_MSG_ID_RADIO_STATUS) {
            mavlink_radio_status_t status;
            mavlink_msg_radio_status_decode(&msg, &status);
            m_lblLinkQuality->setText(QString("%1%").arg(status.rssi));
        }
    }

    void updateMapSmooth() {
        if (m_currentData.lat == 0 && m_currentData.lon == 0) return;

        qint64 now = QDateTime::currentMSecsSinceEpoch();
        double dt = (now - m_lastDataTime) / 1000.0;

        // Veri cok eskiyse veya hiz cok dusukse tahmin yapma
        if (dt > 1.5 || m_currentData.groundSpeed < 0.1) {
            m_map->updatePosition(m_currentData.lat, m_currentData.lon, m_currentData.heading);
            return;
        }

        // Dead Reckoning (Konum Tahmini)
        double R = 6378137.0; // Dunya yaricapi
        double dist = m_currentData.groundSpeed * dt;
        double hdgRad = m_currentData.heading * M_PI / 180.0;
        double dLat = (dist * cos(hdgRad)) / R * (180.0 / M_PI);
        double dLon = (dist * sin(hdgRad)) / (R * cos(m_currentData.lat * M_PI / 180.0)) * (180.0 / M_PI);

        m_map->updatePosition(m_currentData.lat + dLat, m_currentData.lon + dLon, m_currentData.heading);
    }

    void updateTelemetry(const SensorData &data) {
        SensorData displayData = data;
        
        // Hedef Waypoint Bulma
        double targetLat = 0;
        double targetLon = 0;
        bool targetFound = false;
        
        auto items = m_missionManager->getMissionItems();
        for(const auto& wp : items) {
            if(wp.id == m_targetWpIndex) {
                targetLat = wp.lat;
                targetLon = wp.lon;
                targetFound = true;
                break;
            }
        }


        
        // Mesafe ve Aci Hesaplari
        if (targetFound) {
            double R = 6371000;
            double dLat = (targetLat - displayData.lat) * M_PI / 180.0;
            double dLon = (targetLon - displayData.lon) * M_PI / 180.0;
            double a = sin(dLat/2) * sin(dLat/2) + cos(displayData.lat * M_PI / 180.0) * cos(targetLat * M_PI / 180.0) * sin(dLon/2) * sin(dLon/2);
            double c = 2 * atan2(sqrt(a), sqrt(1-a));
            double dist = R * c;
            m_lblDistance->setText(QString("%1 m").arg(dist, 0, 'f', 1));
            
            // Hedef Yaw (Bearing)
            double y = sin(dLon) * cos(targetLat * M_PI / 180.0);
            double x = cos(displayData.lat * M_PI / 180.0) * sin(targetLat * M_PI / 180.0) - 
                       sin(displayData.lat * M_PI / 180.0) * cos(targetLat * M_PI / 180.0) * cos(dLon);
            double bearing = atan2(y, x) * 180.0 / M_PI;
            double targetYaw = fmod(bearing + 360.0, 360.0);
            m_lblTargetYaw->setText(QString::number(targetYaw, 'f', 1) + "°");
            
            // Hata Yaw
            double yawError = targetYaw - displayData.heading;
            if (yawError > 180) yawError -= 360;
            if (yawError < -180) yawError += 360;
            m_lblYawError->setText(QString::number(yawError, 'f', 1) + "°");
        } else {
            m_lblDistance->setText("- m");
            m_lblTargetYaw->setText("-°"); m_lblYawError->setText("-°");
        }

        m_lblLat->setText(QString::number(displayData.lat, 'f', 6));
        m_lblLon->setText(QString::number(data.lon, 'f', 6));
        m_lblAlt->setText(QString::number(data.alt, 'f', 1) + " m");
        m_lblHdg->setText(QString::number(data.heading, 'f', 1) + "°");
        m_lblRoll->setText(QString::number(data.roll * 180.0 / M_PI, 'f', 1) + "°");
        m_lblPitch->setText(QString::number(data.pitch * 180.0 / M_PI, 'f', 1) + "°");
        m_lblSpeed->setText(QString::number(data.groundSpeed, 'f', 1) + " m/s");
        if (data.batteryVoltage > 0) {
            m_lblBatt->setText(QString::number(data.batteryVoltage / 1000.0, 'f', 1) + " V");
        }
        
        m_lblSatCount->setText(QString::number(data.sat_visible));
        m_lblSat->setText(QString::number(data.fix_type));
        m_lblHAcc->setText(QString::number(data.h_acc, 'f', 2) + " m");
        m_lblVAcc->setText(QString::number(data.v_acc, 'f', 2) + " m");
        m_lblGpsCog->setText(QString::number(data.gps_cog, 'f', 1) + "°");
        m_lblGpsHdgAcc->setText(QString::number(data.gps_hdg_acc, 'f', 2) + "°");
        m_lblVehicleAltMsl->setText(QString::number(data.vehicleAltMsl, 'f', 1) + " m");
        
        if(data.rtkHomeSet) {
            m_lblRtkHome->setText(QString("%1, %2").arg(data.rtkHomeLat, 0, 'f', 6).arg(data.rtkHomeLon, 0, 'f', 6));
            m_lblHomeAlt->setText(QString::number(data.homeAlt, 'f', 1) + " m");
            
            QList<Waypoint> items = m_missionManager->getMissionItems();
            if(!items.isEmpty() && items[0].isHome) {
                if(items[0].lat == 0 && items[0].lon == 0) {
                    m_missionManager->updateHomePosition(data.rtkHomeLat, data.rtkHomeLon, 0);
                    m_map->setCenter(data.rtkHomeLat, data.rtkHomeLon);
                }
            }
        }
        else { m_lblRtkHome->setText("Yok"); m_lblHomeAlt->setText("-"); }

        if (data.lat != 0 && data.lon != 0 && !m_hasCenteredVehicle) {
            m_map->runScript(QString("setMapCenter(%1, %2, 15);").arg(data.lat).arg(data.lon));
            m_hasCenteredVehicle = true;
        }
        
        m_lblGpsInput->setText(QString("%1 / %2").arg(data.gps_input_time_week_ms).arg(data.gps_input_vdop, 0, 'f', 2));

        // Mag field
        m_lblMagX->setText(QString::number(data.mag_x));
        m_lblMagY->setText(QString::number(data.mag_y));
        m_lblMagZ->setText(QString::number(data.mag_z));

        // NAV controller
        m_lblNavRoll->setText(QString::number(data.nav_roll, 'f', 1) + "°");
        m_lblNavPitch->setText(QString::number(data.nav_pitch, 'f', 1) + "°");
        m_lblNavBearing->setText(QString::number(data.nav_bearing) + "°");
        m_lblXtrack->setText(QString::number(data.xtrack_error, 'f', 2) + " m");

        // PID
        m_lblPidP->setText(QString::number(data.pidP, 'f', 4));
        m_lblPidI->setText(QString::number(data.pidI, 'f', 4));
        m_lblPidD->setText(QString::number(data.pidD, 'f', 4));
        if(data.armed) {
            m_lblArm->setText("ARMED"); m_lblArm->setStyleSheet("color: #f7768e; font-size: 16px; font-weight: bold; border: none;");
        } else {
            m_lblArm->setText("DISARMED"); m_lblArm->setStyleSheet("color: #9ece6a; font-size: 16px; font-weight: bold; border: none;");
        }
        m_hudOverlay->pfd->setAttitude(data.roll, data.pitch);
        m_hudOverlay->compass->setHeading(data.heading);
        
        // Harita guncellemesini timer'a birak (interpolasyon icin)
        m_currentData = displayData;
        m_lastDataTime = QDateTime::currentMSecsSinceEpoch();
        
        m_plotRoll->addData(data.roll * 180.0 / M_PI);
        m_plotPitch->addData(data.pitch * 180.0 / M_PI);
        m_plotSpeed->addData(data.groundSpeed);
        m_alertLabel->setText(data.statusText);
    }
    

};

#include "main.moc"

int main(int argc, char *argv[]) {
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--disable-ipc-flooding-protection");
    QApplication app(argc, argv);
    setlocale(LC_NUMERIC, "C");
    QLocale::setDefault(QLocale::C);
    app.setOrganizationName("MavUI");
    app.setApplicationName("mavui");
    QFile file(":/config/tokyo_night.qss");
    if(file.open(QFile::ReadOnly)) {
        QString styleSheet = QLatin1String(file.readAll());
        app.setStyleSheet(styleSheet);
    }
    MainWindow w;
    w.show();
    return app.exec();
}