#include "settings.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <QPainter>
#include <QMouseEvent>
#include <QCoreApplication>
#include <QFile>
#include <QGroupBox>

class SettingsDialog : public QDialog {
public:
    SettingsDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground);
        resize(500, 700);
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        Q_UNUSED(event);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        
        p.setBrush(QColor(26, 27, 38, 250)); 
        p.setPen(QPen(QColor(122, 162, 247), 2)); 
        p.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 16, 16);
    }

    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
            event->accept();
        }
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        if (event->buttons() & Qt::LeftButton) {
            move(event->globalPosition().toPoint() - m_dragPos);
            event->accept();
        }
    }

private:
    QPoint m_dragPos;
};

Settings::Settings(QObject *parent) : QObject(parent) {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configDir);
    
    QString userConfigPath = configDir + "/config.ini";
    QString defaultConfigPath = QCoreApplication::applicationDirPath() + "/config/default.ini";

    if (!QFile::exists(userConfigPath)) {
        if (QFile::exists(defaultConfigPath)) {
            QFile::copy(defaultConfigPath, userConfigPath);
            QFile::setPermissions(userConfigPath, QFile::ReadOwner | QFile::WriteOwner);
        }
    }

    m_store = new QSettings(userConfigPath, QSettings::IniFormat, this);
    load();
}

void Settings::load() {
    m_defaultHome = m_store->value("General/default_home", "40.790709,29.980958,0").toString();
    m_mapTheme = m_store->value("General/map_theme", "Uydu (Satellite)").toString();
    
    m_leftSidebarWidth = m_store->value("Bars/left_sidebar_width", 250).toInt();
    m_sidebarWidth = m_store->value("Bars/right_sidebar_width", 250).toInt();
    m_bottomBarHeight = m_store->value("Bars/bottom_height", 60).toInt();

    m_camViewTR = m_store->value("General/CamViewTR", "42.0,45.0").toString();
    m_camViewBL = m_store->value("General/CamViewBL", "36.0,26.0").toString();

    m_mavBaud = m_store->value("Connections/mav_baud", 115200).toInt();
    m_mavPort = m_store->value("Connections/mav_port", "ttyACM1").toString();
    m_rtkBaud = m_store->value("Connections/rtk_baud", 115200).toInt();
    m_rtkPort = m_store->value("Connections/rtk_port", "ttyACM0").toString();
}

void Settings::save() {
    m_store->setValue("General/default_home", m_defaultHome);
    m_store->setValue("General/map_theme", m_mapTheme);
    
    m_store->setValue("Bars/left_sidebar_width", m_leftSidebarWidth);
    m_store->setValue("Bars/right_sidebar_width", m_sidebarWidth);
    m_store->setValue("Bars/bottom_height", m_bottomBarHeight);

    m_store->setValue("General/CamViewTR", m_camViewTR);
    m_store->setValue("General/CamViewBL", m_camViewBL);

    m_store->setValue("Connections/mav_baud", m_mavBaud);
    m_store->setValue("Connections/mav_port", m_mavPort);
    m_store->setValue("Connections/rtk_baud", m_rtkBaud);
    m_store->setValue("Connections/rtk_port", m_rtkPort);
    
    m_store->sync();
    emit settingsChanged();
}

QString Settings::getDefaultHome() const { return m_defaultHome; }
void Settings::setDefaultHome(const QString &val) { m_defaultHome = val; }

QString Settings::getMapTheme() const { return m_mapTheme; }
void Settings::setMapTheme(const QString &theme) { m_mapTheme = theme; save(); }

int Settings::getLeftSidebarWidth() const { return m_leftSidebarWidth; }
void Settings::setLeftSidebarWidth(int width) { m_leftSidebarWidth = width; save(); }

int Settings::getSidebarWidth() const { return m_sidebarWidth; }
void Settings::setSidebarWidth(int width) { m_sidebarWidth = width; save(); }

int Settings::getBottomBarHeight() const { return m_bottomBarHeight; }
void Settings::setBottomBarHeight(int height) { m_bottomBarHeight = height; save(); }

QString Settings::getCamViewTR() const { return m_camViewTR; }
void Settings::setCamViewTR(const QString &coords) { m_camViewTR = coords; }

QString Settings::getCamViewBL() const { return m_camViewBL; }
void Settings::setCamViewBL(const QString &coords) { m_camViewBL = coords; }

int Settings::getMavBaud() const { return m_mavBaud; }
void Settings::setMavBaud(int baud) { m_mavBaud = baud; }

QString Settings::getMavPort() const { return m_mavPort; }
void Settings::setMavPort(const QString &port) { m_mavPort = port; }

int Settings::getRtkBaud() const { return m_rtkBaud; }
void Settings::setRtkBaud(int baud) { m_rtkBaud = baud; }

QString Settings::getRtkPort() const { return m_rtkPort; }
void Settings::setRtkPort(const QString &port) { m_rtkPort = port; }

void Settings::exec() {
    QWidget *parentWidget = qobject_cast<QWidget*>(parent());
    SettingsDialog dlg(parentWidget);
    
    QString style = R"(
        QLabel { color: #a9b1d6; font-weight: bold; font-size: 14px; margin-bottom: 5px; }
        QLineEdit { 
            background-color: #16161e; color: #c0caf5; 
            border: 1px solid #565f89; border-radius: 8px; 
            padding: 8px; font-size: 13px; margin-bottom: 5px;
        }
        QLineEdit:focus { border: 1px solid #7aa2f7; }
        QComboBox { 
            background-color: #16161e; color: #c0caf5; 
            border: 1px solid #565f89; border-radius: 8px; 
            padding: 8px; font-size: 13px; margin-bottom: 5px;
        }
        QComboBox::drop-down { border: none; }
        QPushButton {
            background-color: #3b4261; color: white;
            border-radius: 8px; padding: 10px;
            font-weight: bold; font-size: 12px;
        }
        QPushButton:hover { background-color: #444b6a; }
        QPushButton#saveBtn { background-color: #7aa2f7; color: #15161e; }
        QPushButton#cancelBtn { background-color: #f7768e; color: #15161e; }
        QPushButton#clearBtn { background-color: #ff9e64; color: #15161e; }
        QPushButton#importBtn { background-color: #9ece6a; color: #15161e; }
        QGroupBox { border: 1px solid #565f89; border-radius: 6px; margin-top: 10px; padding-top: 10px; }
        QGroupBox::title { color: #bb9af7; subcontrol-origin: margin; left: 10px; padding: 0 3px 0 3px; }
    )";
    dlg.setStyleSheet(style);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dlg);
    mainLayout->setContentsMargins(20, 20, 20, 20); 
    mainLayout->setSpacing(10);

    QLabel *title = new QLabel("AYARLAR");
    title->setStyleSheet("color: #7aa2f7; font-size: 18px; font-weight: bold; margin-bottom: 15px;");
    title->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(title);

    QFormLayout *form = new QFormLayout();
    form->setSpacing(10);

    QLineEdit *edtDefaultHome = new QLineEdit(m_defaultHome);
    edtDefaultHome->setPlaceholderText("Lat,Lon,Alt");
    
    QComboBox *cmbTheme = new QComboBox();
    cmbTheme->addItems({"Uydu (Satellite)", "Sokak (Street)", "Karanlik (Dark)"});
    cmbTheme->setCurrentText(m_mapTheme);

    QLineEdit *edtCamTR = new QLineEdit(m_camViewTR);
    QLineEdit *edtCamBL = new QLineEdit(m_camViewBL);

    QLineEdit *edtMavPort = new QLineEdit(m_mavPort);
    QLineEdit *edtMavBaud = new QLineEdit(QString::number(m_mavBaud));
    QLineEdit *edtRtkPort = new QLineEdit(m_rtkPort);
    QLineEdit *edtRtkBaud = new QLineEdit(QString::number(m_rtkBaud));

    form->addRow("Default Home:", edtDefaultHome);
    form->addRow("Harita Temasi:", cmbTheme);
    form->addRow("CamView TR:", edtCamTR);
    form->addRow("CamView BL:", edtCamBL);
    form->addRow("MAV Port:", edtMavPort);
    form->addRow("MAV Baud:", edtMavBaud);
    form->addRow("RTK Port:", edtRtkPort);
    form->addRow("RTK Baud:", edtRtkBaud);

    mainLayout->addLayout(form);

    QGroupBox *grpImport = new QGroupBox("Hizli Waypoint Ekle");
    QVBoxLayout *importLayout = new QVBoxLayout(grpImport);
    QLineEdit *edtImport = new QLineEdit();
    edtImport->setPlaceholderText("String yapistir: ~Lat,Lon,Alt,Rad*...");
    QPushButton *btnImport = new QPushButton("String Import Et");
    btnImport->setObjectName("importBtn");
    importLayout->addWidget(edtImport);
    importLayout->addWidget(btnImport);
    mainLayout->addWidget(grpImport);

    connect(btnImport, &QPushButton::clicked, this, [this, edtImport, &dlg](){
        if(!edtImport->text().isEmpty()) {
            emit importStringRequested(edtImport->text());
            QMessageBox::information(&dlg, "Basarili", "Veriler import edildi.");
            edtImport->clear();
        }
    });

    QPushButton *btnClear = new QPushButton("Varsayilan Gorev Dosyasini Sil");
    btnClear->setObjectName("clearBtn");
    connect(btnClear, &QPushButton::clicked, this, [this](){
        emit clearFilesRequested();
        QMessageBox::information(nullptr, "Bilgi", "Varsayilan gorev dosyalari silindi.");
    });
    mainLayout->addWidget(btnClear);

    mainLayout->addStretch();

    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *btnSave = new QPushButton("KAYDET");
    btnSave->setObjectName("saveBtn");
    QPushButton *btnCancel = new QPushButton("IPTAL");
    btnCancel->setObjectName("cancelBtn");

    btnLayout->addWidget(btnCancel);
    btnLayout->addWidget(btnSave);
    mainLayout->addLayout(btnLayout);

    connect(btnSave, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        m_defaultHome = edtDefaultHome->text();
        m_mapTheme = cmbTheme->currentText();
        m_camViewTR = edtCamTR->text();
        m_camViewBL = edtCamBL->text();
        m_mavPort = edtMavPort->text();
        m_mavBaud = edtMavBaud->text().toInt();
        m_rtkPort = edtRtkPort->text();
        m_rtkBaud = edtRtkBaud->text().toInt();
        save();
    }
}