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
#include <algorithm>

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
    MainWindow(QWidget *parent = nullptr) : QMainWindow(parent), m_isRtkConnected(false) {
        m_settings = new Settings(this);
        m_connection = new Connection(this);
        m_logViewer = new LogViewer(this);
        m_csvManager = new CsvManager(this);
        m_sensorPub = new SensorPub(this);
        m_missionManager = new MissionManager(this);
        m_rtkSerial = new QSerialPort(this);
        
        setupUI();
        
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
             m_inputLat->setText(QString::number(lat, 'f', 6));
             m_inputLon->setText(QString::number(lon, 'f', 6));
             
             if(m_btnPickMapWp->isChecked()) {
                 m_missionManager->addTargetWaypoint(lat, lon, 20.0);
             }
        });
        
        connect(m_map, &MapWidget::zoneClicked, this, [this](double lat, double lon){
            m_inputLat->setText(QString::number(lat, 'f', 6));
            m_inputLon->setText(QString::number(lon, 'f', 6));
            if(m_btnPickMapZone->isChecked()) {
                m_missionManager->addZonePoint(lat, lon);
            }
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
                if (wp.id == 0) m_missionManager->updateHomePosition(wp.lat, wp.lon, wp.alt);
                else m_missionManager->addTargetWaypoint(wp.lat, wp.lon, wp.alt);
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
        if (m_bottomDock && m_bottomDock->isVisible()) {
            m_settings->setBottomBarHeight(m_bottomDock->height());
        }
        QMainWindow::closeEvent(event);
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
    QDockWidget *m_bottomDock;
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
    
    GraphWindow *m_graphWindow;
    SimplePlot *m_plotRoll;
    SimplePlot *m_plotPitch;
    SimplePlot *m_plotSpeed;
    
    QLabel *m_lblLat, *m_lblLon, *m_lblAlt, *m_lblHdg, *m_lblSpeed, *m_lblBatt, *m_lblArm;
    QLabel *m_lblSat, *m_lblAcc, *m_lblRtkHome, *m_lblGpsInput; 
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
        m_hudOverlay = new HUDOverlay(m_map);
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
        actionRow1->addWidget(m_btnSaveItem);
        actionRow1->addWidget(m_btnAddWp);
        actionRow1->addWidget(m_btnAddZonePoint);
        leftLayout->addLayout(actionRow1);

        m_btnCompleteZone = new QPushButton("Zone'u Tamamla");
        leftLayout->addWidget(m_btnCompleteZone);

        QHBoxLayout *actionRow3 = new QHBoxLayout();
        m_btnPickMapZone = new QPushButton("Haritadan Zone");
        m_btnPickMapZone->setCheckable(true);
        m_btnPickMapWp = new QPushButton("Haritadan WP");
        m_btnPickMapWp->setCheckable(true);
        actionRow3->addWidget(m_btnPickMapZone);
        actionRow3->addWidget(m_btnPickMapWp);
        leftLayout->addLayout(actionRow3);

        leftLayout->addSpacing(10);

        QHBoxLayout *listEditLayout = new QHBoxLayout();
        m_btnEditItem = new QPushButton("Duzenle");
        m_btnDeleteItem = new QPushButton("Sil");
        m_btnDeleteItem->setStyleSheet("background-color: #f7768e;");
        listEditLayout->addWidget(m_btnEditItem);
        listEditLayout->addWidget(m_btnDeleteItem);
        leftLayout->addLayout(listEditLayout);

        m_listTabs = new QTabWidget();
        m_listTabs->setStyleSheet("QTabWidget::pane { border: 1px solid #565f89; }");
        m_listWaypoints = new QListWidget();
        m_listZones = new QListWidget();
        m_listTabs->addTab(m_listWaypoints, "Waypoints");
        m_listTabs->addTab(m_listZones, "Zones");
        leftLayout->addWidget(m_listTabs);

        QGroupBox *grpOps = new QGroupBox("Islemler");
        QVBoxLayout *opsLayout = new QVBoxLayout(grpOps);
        m_btnSaveDefaults = new QPushButton("Varsayilana Kaydet");
        m_btnSaveDefaults->setStyleSheet("background-color: #7aa2f7; color: #15161e;");
        opsLayout->addWidget(m_btnSaveDefaults);

        QHBoxLayout *vehBtns = new QHBoxLayout();
        m_btnUploadVehicle = new QPushButton("Araca Yukle");
        m_btnDownloadVehicle = new QPushButton("Aractan Indir");
        vehBtns->addWidget(m_btnUploadVehicle); vehBtns->addWidget(m_btnDownloadVehicle);
        opsLayout->addLayout(vehBtns);

        QHBoxLayout *fileBtns = new QHBoxLayout();
        m_btnSaveFile = new QPushButton("Dosyaya Kaydet");
        m_btnLoadFile = new QPushButton("Dosyadan Yukle");
        fileBtns->addWidget(m_btnSaveFile); fileBtns->addWidget(m_btnLoadFile);
        opsLayout->addLayout(fileBtns);
        
        m_btnClearMission = new QPushButton("Gorevi Temizle");
        opsLayout->addWidget(m_btnClearMission);
        leftLayout->addWidget(grpOps);

        QWidget *rightSidebar = new QWidget(this);
        QVBoxLayout *rightLayout = new QVBoxLayout(rightSidebar);
        rightLayout->setContentsMargins(10, 10, 10, 10);
        rightLayout->setSpacing(10);

        m_connectionTabs = new QTabWidget(this);
        m_connectionTabs->setFixedHeight(220);
        
        QWidget *mavTab = new QWidget();
        QVBoxLayout *mavLayout = new QVBoxLayout(mavTab);
        m_cmbMavPort = new QComboBox();
        m_cmbMavBaud = new QComboBox();
        QList<int> rates = {9600, 19200, 38400, 57600, 115200};
        for(int r : rates) m_cmbMavBaud->addItem(QString::number(r));
        m_cmbMavBaud->setCurrentText("57600");
        m_btnMavRefresh = new QPushButton("Yenile");
        m_btnMavConnect = new QPushButton("BAGLAN");
        m_btnMavConnect->setStyleSheet("background-color: #9ece6a; color: white; font-weight: bold; padding: 8px;");
        
        mavLayout->addWidget(new QLabel("Port:"));
        QHBoxLayout *mavPl = new QHBoxLayout(); mavPl->addWidget(m_cmbMavPort); mavPl->addWidget(m_btnMavRefresh); mavLayout->addLayout(mavPl);
        mavLayout->addWidget(new QLabel("Baud:")); mavLayout->addWidget(m_cmbMavBaud); mavLayout->addWidget(m_btnMavConnect); mavLayout->addStretch();
        
        QWidget *rtkTab = new QWidget();
        QVBoxLayout *rtkLayout = new QVBoxLayout(rtkTab);
        m_cmbRtkPort = new QComboBox();
        m_cmbRtkBaud = new QComboBox();
        for(int r : rates) m_cmbRtkBaud->addItem(QString::number(r));
        m_cmbRtkBaud->setCurrentText("57600");
        m_btnRtkRefresh = new QPushButton("Yenile");
        m_btnRtkConnect = new QPushButton("BAGLAN");
        m_btnRtkConnect->setStyleSheet("background-color: #9ece6a; color: white; font-weight: bold; padding: 8px;");
        
        rtkLayout->addWidget(new QLabel("Port:"));
        QHBoxLayout *rtkPl = new QHBoxLayout(); rtkPl->addWidget(m_cmbRtkPort); rtkPl->addWidget(m_btnRtkRefresh); rtkLayout->addLayout(rtkPl);
        rtkLayout->addWidget(new QLabel("Baud:")); rtkLayout->addWidget(m_cmbRtkBaud); rtkLayout->addWidget(m_btnRtkConnect); rtkLayout->addStretch();

        m_connectionTabs->addTab(mavTab, "MAVLink");
        m_connectionTabs->addTab(rtkTab, "RTK");
        rightLayout->addWidget(m_connectionTabs);

        QHBoxLayout *rightBtnLayout = new QHBoxLayout();
        QPushButton *btnSettings = new QPushButton("AYARLAR");
        QPushButton *btnLogs = new QPushButton("LOGLAR");
        rightBtnLayout->addWidget(btnSettings);
        rightBtnLayout->addWidget(btnLogs);
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
            return frame;
        };

        m_lblLat = new QLabel("0.000000"); m_lblLon = new QLabel("0.000000"); 
        m_lblAlt = new QLabel("0.0 m"); m_lblHdg = new QLabel("0°");
        m_lblSpeed = new QLabel("0.0 m/s"); m_lblBatt = new QLabel("0.0 V");
        m_lblSat = new QLabel("0"); m_lblArm = new QLabel("DISARMED");
        m_lblAcc = new QLabel("-"); m_lblRtkHome = new QLabel("Yok"); m_lblGpsInput = new QLabel("-");

        QGridLayout *gridLayout = new QGridLayout();
        gridLayout->setSpacing(10);
        gridLayout->addWidget(createStatBox("ENLEM", m_lblLat), 0, 0); gridLayout->addWidget(createStatBox("BOYLAM", m_lblLon), 0, 1);
        gridLayout->addWidget(createStatBox("YUKSEKLIK", m_lblAlt), 1, 0); gridLayout->addWidget(createStatBox("HEADING", m_lblHdg), 1, 1);
        gridLayout->addWidget(createStatBox("HIZ", m_lblSpeed), 2, 0); gridLayout->addWidget(createStatBox("BATARYA", m_lblBatt), 2, 1);
        gridLayout->addWidget(createStatBox("UYDU / FIX", m_lblSat), 3, 0);
        gridLayout->addWidget(createStatBox("HASSASIYET", m_lblAcc), 4, 0); gridLayout->addWidget(createStatBox("RTK HOME", m_lblRtkHome), 4, 1);
        QFrame* gpsInputFrame = createStatBox("GPS INPUT", m_lblGpsInput); gridLayout->addWidget(gpsInputFrame, 5, 0, 1, 2);

        QFrame* armFrame = new QFrame();
        armFrame->setStyleSheet("background-color: #1f2335; border-radius: 6px; border: 1px solid #3b4261;");
        QHBoxLayout* armLayout = new QHBoxLayout(armFrame);
        QLabel* armTitle = new QLabel("DURUM:");
        armTitle->setStyleSheet("color: #787c99; font-size: 12px; font-weight: bold; border: none;");
        m_lblArm->setStyleSheet("color: #9ece6a; font-size: 16px; font-weight: bold; border: none;");
        m_lblArm->setAlignment(Qt::AlignCenter);
        armLayout->addWidget(armTitle); armLayout->addStretch(); armLayout->addWidget(m_lblArm); armLayout->addStretch();
        gridLayout->addWidget(armFrame, 6, 0, 1, 2);
        rightLayout->addLayout(gridLayout);
        
        QPushButton *btnOpenGraph = new QPushButton("GRAFIKLER");
        btnOpenGraph->setStyleSheet("background-color: #7aa2f7; color: #1a1b26; font-weight: bold; padding: 15px; margin-top: 10px;");
        connect(btnOpenGraph, &QPushButton::clicked, m_graphWindow, &GraphWindow::show);
        rightLayout->addWidget(btnOpenGraph);
        rightLayout->addStretch();

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

        m_bottomDock = new QDockWidget("Kontrol", this);
        m_bottomDock->setTitleBarWidget(new QWidget());
        QWidget *dockWidget = new QWidget();
        QHBoxLayout *dockLayout = new QHBoxLayout(dockWidget);
        dockLayout->setContentsMargins(0, 0, 0, 0); dockLayout->addStretch();
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
        QLabel *lblBottomLogo = new QLabel("AURA");
        lblBottomLogo->setStyleSheet("font-family: 'Segoe UI'; font-weight: bold; color: #7aa2f7; font-size: 20px; margin-left: 15px; border-left: 1px solid #3b4261; padding-left: 10px; margin-right: 10px;");
        dockLayout->addWidget(lblBottomLogo);
        m_bottomDock->setWidget(dockWidget);
        addDockWidget(Qt::BottomDockWidgetArea, m_bottomDock);

        connect(m_btnMavConnect, &QPushButton::clicked, this, &MainWindow::toggleConnection);
        connect(m_btnMavRefresh, &QPushButton::clicked, this, [this](){ refreshPorts(m_cmbMavPort); });
        connect(m_btnRtkConnect, &QPushButton::clicked, this, [this](){
            if(m_btnRtkConnect->text() == "BAGLAN") {
                m_rtkSerial->setPortName(m_cmbRtkPort->currentText());
                m_rtkSerial->setBaudRate(m_cmbRtkBaud->currentText().toInt());
                if(m_rtkSerial->open(QIODevice::ReadOnly)) {
                    m_btnRtkConnect->setText("KES");
                    m_btnRtkConnect->setStyleSheet("background-color: #f7768e; color: white;");
                    m_isRtkConnected = true;
                } else QMessageBox::warning(this, "Hata", "RTK baglantisi acilamadi!");
            } else {
                m_rtkSerial->close();
                m_btnRtkConnect->setText("BAGLAN");
                m_btnRtkConnect->setStyleSheet("background-color: #9ece6a; color: white;");
                m_isRtkConnected = false;
            }
        });
        connect(m_btnRtkRefresh, &QPushButton::clicked, this, [this](){ refreshPorts(m_cmbRtkPort); });
        connect(btnSettings, &QPushButton::clicked, m_settings, &Settings::exec);
        connect(btnLogs, &QPushButton::clicked, m_logViewer, &LogViewer::exec);
        
        connect(m_btnPickMapWp, &QPushButton::clicked, [this](bool checked){
            m_map->setClickMode(checked);
            if(checked) m_btnPickMapZone->setChecked(false);
        });
        connect(m_btnPickMapZone, &QPushButton::clicked, [this](bool checked){
            m_map->setZoneMode(checked);
            if(checked) m_btnPickMapWp->setChecked(false);
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
            m_missionManager->addTargetWaypoint(lat, lon, alt);
            
            double rad = m_inputRadius->text().toDouble();
            if(rad > 0) {
                 int lastId = m_missionManager->getMissionItems().last().id;
                 m_missionManager->addCircleToWaypoint(lastId, rad);
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
            m_missionManager->addZonePoint(lat, lon);
        });
        connect(m_btnCompleteZone, &QPushButton::clicked, m_missionManager, &MissionManager::completeZone);
        
        connect(m_btnSaveDefaults, &QPushButton::clicked, m_missionManager, &MissionManager::saveDefaults);
        connect(m_btnClearMission, &QPushButton::clicked, m_missionManager, &MissionManager::clearAll);
        connect(m_btnUploadVehicle, &QPushButton::clicked, this, [this](){ m_connection->uploadMission(m_missionManager->getMissionItems()); });
        connect(m_btnDownloadVehicle, &QPushButton::clicked, m_connection, &Connection::downloadMission);
        connect(m_btnSaveFile, &QPushButton::clicked, this, [this](){
            QString path = QFileDialog::getSaveFileName(this, "Kaydet", "", "JSON (*.json)");
            if(!path.isEmpty()) m_missionManager->saveMissionToFile(path);
        });
        connect(m_btnLoadFile, &QPushButton::clicked, this, [this](){
            QString path = QFileDialog::getOpenFileName(this, "Yukle", "", "JSON (*.json)");
            if(!path.isEmpty()) m_missionManager->loadMissionFromFile(path);
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
                         m_inputLat->setText(QString::number(wp.lat, 'f', 6));
                         m_inputLon->setText(QString::number(wp.lon, 'f', 6));
                         m_inputAlt->setText(QString::number(wp.alt));
                         if(wp.radius > 0) m_inputRadius->setText(QString::number(wp.radius));
                         else m_inputRadius->clear();
                         break;
                     }
                 }
             }
        });

        refreshPorts(m_cmbMavPort); refreshPorts(m_cmbRtkPort);
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
        if(m_settings->getBottomBarHeight() > 0) resizeDocks({m_bottomDock}, {m_settings->getBottomBarHeight()}, Qt::Vertical);
        
        m_map->runScript(QString("setMapTheme('%1');").arg(m_settings->getMapTheme()));
        
        QString defaultHome = m_settings->getDefaultHome();
        QStringList parts = defaultHome.split(',');
        if(parts.size() >= 3) {
            double lat = parts[0].toDouble();
            double lon = parts[1].toDouble();
            double alt = parts[2].toDouble();
            m_missionManager->updateHomePosition(lat, lon, alt);
            m_map->setCenter(lat, lon);
        }
    }
    
    void refreshLists() {
        m_listWaypoints->clear();
        for(const auto& wp : m_missionManager->getMissionItems()) {
            QString prefix = (wp.id == 0) ? "HOME" : QString("WP %1").arg(wp.id);
            QString txt = QString("%1: %2, %3 (%4m)").arg(prefix).arg(wp.lat, 0, 'f', 6).arg(wp.lon, 0, 'f', 6).arg(wp.alt);
            if(wp.radius > 0) txt += QString(" [R: %1m]").arg(wp.radius);
            QListWidgetItem *item = new QListWidgetItem(txt);
            item->setData(Qt::UserRole, wp.id);
            if(wp.id == 0) item->setForeground(Qt::green);
            m_listWaypoints->addItem(item);
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
            m_btnMavConnect->setStyleSheet("background-color: #9ece6a; color: white; font-weight: bold; padding: 8px;");
            m_statusLabel->setText("BAGLANTI KESILDI");
            m_statusLabel->setStyleSheet("font-weight: bold; color: #f7768e; font-size: 14px; background: transparent;");
        } else {
            QString portName = m_cmbMavPort->currentText();
            if(portName == "(Port Yok)") return;
            int baudRate = m_cmbMavBaud->currentText().toInt();
            if (m_connection->connectMavlink(portName, baudRate)) {
                m_btnMavConnect->setText("KES");
                m_btnMavConnect->setStyleSheet("background-color: #f7768e; color: white; font-weight: bold; padding: 8px;");
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
            m_btnMavConnect->setStyleSheet("background-color: #9ece6a; color: white; font-weight: bold; padding: 8px;");
            m_statusLabel->setText("BAGLANTI KOPTU");
            m_statusLabel->setStyleSheet("font-weight: bold; color: #f7768e; font-size: 14px; background: transparent;");
        }
    }

    void processMavlinkMessage(const mavlink_message_t &msg) {
        if (m_sensorPub) m_sensorPub->updateData(msg);
    }

    void updateTelemetry(const SensorData &data) {
        m_lblLat->setText(QString::number(data.lat, 'f', 6));
        m_lblLon->setText(QString::number(data.lon, 'f', 6));
        m_lblAlt->setText(QString::number(data.alt, 'f', 1) + " m");
        m_lblHdg->setText(QString::number(data.heading, 'f', 1) + "°");
        m_lblSpeed->setText(QString::number(data.groundSpeed, 'f', 1) + " m/s");
        m_lblBatt->setText(QString::number(data.batteryVoltage, 'f', 1) + " V");
        m_lblSat->setText(QString("%1 / %2").arg(data.sat_visible).arg(data.fix_type));
        m_lblAcc->setText(QString("H:%1 V:%2").arg(data.h_acc, 0, 'f', 2).arg(data.v_acc, 0, 'f', 2));
        if(data.rtkHomeSet) m_lblRtkHome->setText(QString("%1, %2").arg(data.rtkHomeLat, 0, 'f', 6).arg(data.rtkHomeLon, 0, 'f', 6));
        else m_lblRtkHome->setText("Yok");
        m_lblGpsInput->setText(QString("%1 / %2").arg(data.gps_input_time_week_ms).arg(data.gps_input_vdop, 0, 'f', 2));
        if(data.armed) {
            m_lblArm->setText("ARMED"); m_lblArm->setStyleSheet("color: #f7768e; font-size: 16px; font-weight: bold; border: none;");
        } else {
            m_lblArm->setText("DISARMED"); m_lblArm->setStyleSheet("color: #9ece6a; font-size: 16px; font-weight: bold; border: none;");
        }
        m_hudOverlay->pfd->setAttitude(data.roll, data.pitch);
        m_hudOverlay->compass->setHeading(data.heading);
        m_map->updatePosition(data.lat, data.lon, data.heading);
        m_plotRoll->addData(data.roll * 180.0 / M_PI);
        m_plotPitch->addData(data.pitch * 180.0 / M_PI);
        m_plotSpeed->addData(data.groundSpeed);
        m_alertLabel->setText(data.statusText);
    }
};

#include "main.moc"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QFile file(":/config/tokyo_night.qss");
    if(file.open(QFile::ReadOnly)) {
        QString styleSheet = QLatin1String(file.readAll());
        app.setStyleSheet(styleSheet);
    }
    MainWindow w;
    w.show();
    return app.exec();
}