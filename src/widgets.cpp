#include "widgets.h"
#include "missionmanager.h" 
#include <QPainter>
#include <QPen>
#include <QPainterPath>
#include <cmath>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QUrl>
#include <QMouseEvent>
#include <QScrollArea>

#ifdef USE_WEBENGINE
    #include <QWebEngineView>
    #include <QWebEnginePage>
#else
    #include <QLabel>
#endif

HUDOverlay::HUDOverlay(QWidget *parent) : QWidget(parent) {
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 5, 10, 5);
    layout->setSpacing(15);
    pfd = new PFDWidget(this);
    compass = new CompassWidget(this);
    layout->addWidget(pfd);
    layout->addWidget(compass);
    setLayout(layout);
    setAttribute(Qt::WA_TranslucentBackground);
}

void HUDOverlay::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(20, 20, 25, 200));
    p.setPen(QPen(QColor(65, 72, 104), 2));
    p.drawRoundedRect(rect(), 15, 15);
}

PFDWidget::PFDWidget(QWidget *parent) : QWidget(parent), m_roll(0), m_pitch(0) { 
    setFixedSize(140, 140); 
    setAttribute(Qt::WA_TranslucentBackground); 
}

void PFDWidget::setAttitude(float roll, float pitch) { 
    m_roll = roll * 180.0 / M_PI; 
    m_pitch = pitch * 180.0 / M_PI; 
    update(); 
}

void PFDWidget::paintEvent(QPaintEvent *) {
    QPainter p(this); 
    p.setRenderHint(QPainter::Antialiasing);
    int w = width(); int h = height(); int r = (qMin(w, h) / 2) - 5;
    p.translate(w / 2, h / 2);
    QPainterPath path; path.addEllipse(-r, -r, 2 * r, 2 * r); 
    p.setClipPath(path);
    p.save(); 
    p.rotate(-m_roll); 
    p.translate(0, m_pitch * 4);
    p.fillRect(-r*4, -r*4, r*8, r*4, QColor(0, 120, 200)); 
    p.fillRect(-r*4, 0, r*8, r*4, QColor(34, 139, 34)); 
    p.setPen(QPen(Qt::white, 2)); p.drawLine(-r*4, 0, r*4, 0); 
    p.restore();
    p.setPen(QPen(Qt::black, 3)); 
    p.drawLine(-20, 0, -5, 0); p.drawLine(5, 0, 20, 0); p.drawPoint(0, 0);
    p.setClipping(false); 
    p.setPen(QPen(QColor(200, 200, 200), 3)); 
    p.setBrush(Qt::NoBrush); 
    p.drawEllipse(-r, -r, 2 * r, 2 * r);
}

CompassWidget::CompassWidget(QWidget *parent) : QWidget(parent), m_heading(0) { 
    setFixedSize(140, 140); 
    setAttribute(Qt::WA_TranslucentBackground); 
}

void CompassWidget::setHeading(float heading) { 
    m_heading = heading; 
    update(); 
}

void CompassWidget::paintEvent(QPaintEvent *) {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    int r = (qMin(width(), height()) / 2) - 5; 
    p.translate(width() / 2, height() / 2);
    p.setBrush(QColor(30, 30, 35)); p.setPen(QPen(QColor(200, 200, 200), 3)); 
    p.drawEllipse(-r, -r, 2 * r, 2 * r);
    p.save(); p.rotate(-m_heading); p.setFont(QFont("Arial", 10, QFont::Bold));
    p.setPen(Qt::red); static const QPoint northTri[] = { QPoint(-5, -r+5), QPoint(5, -r+5), QPoint(0, -r+15) }; 
    p.drawPolygon(northTri, 3);
    p.drawText(-10, -r + 20, 20, 20, Qt::AlignCenter, "N");
    p.setPen(Qt::white); 
    p.drawText(-10, r - 35, 20, 20, Qt::AlignCenter, "S"); 
    p.drawText(r - 35, -10, 20, 20, Qt::AlignCenter, "E"); 
    p.drawText(-r + 15, -10, 20, 20, Qt::AlignCenter, "W");
    for(int i = 0; i < 360; i+=30) { p.drawLine(0, -r, 0, -r + 8); p.rotate(30); } 
    p.restore();
    p.setPen(Qt::yellow); p.setFont(QFont("Arial", 14, QFont::Bold)); 
    p.drawText(-40, -15, 80, 30, Qt::AlignCenter, QString::number((int)m_heading) + "°");
}

SimplePlot::SimplePlot(QString title, QWidget *parent) : QWidget(parent), m_title(title), m_maxDataPoints(200), m_minY(0), m_maxY(1) {
    setMinimumHeight(150);
}

void SimplePlot::addData(double value) {
    m_data.append(value); if (m_data.size() > m_maxDataPoints) m_data.removeFirst();
    
    if (m_data.size() > 0) {
        double currentMin = m_data[0];
        double currentMax = m_data[0];
        for(double v : m_data) {
            if(v < currentMin) currentMin = v;
            if(v > currentMax) currentMax = v;
        }
        m_minY = currentMin; 
        m_maxY = currentMax;
        
        if(std::abs(m_maxY - m_minY) < 0.0001) { 
            m_maxY += 0.5; 
            m_minY -= 0.5; 
        }
    }
    update();
}

void SimplePlot::paintEvent(QPaintEvent *) {
    QPainter p(this); 
    p.fillRect(rect(), QColor(26, 27, 38)); 
    
    p.setPen(QColor(169, 177, 214)); 
    p.setFont(QFont("Arial", 10, QFont::Bold));
    p.drawText(10, 20, m_title);

    if (m_data.isEmpty()) return;

    double currentVal = m_data.last();
    p.setPen(QColor(0, 255, 0)); 
    p.setFont(QFont("Arial", 12, QFont::Bold));
    p.drawText(10, height() / 2, QString::number(currentVal, 'f', 2));

    p.setPen(QColor(200, 200, 200));
    p.setFont(QFont("Arial", 8));
    p.drawText(5, 35, QString("Max: %1").arg(m_maxY, 0, 'f', 2));
    p.drawText(5, height() - 5, QString("Min: %1").arg(m_minY, 0, 'f', 2));

    if (m_data.size() < 2) return; 
    
    p.setPen(QPen(QColor(122, 162, 247), 2));
    
    int leftMargin = 60; 
    int rightMargin = 10;
    int topMargin = 40;
    int bottomMargin = 20;
    int graphHeight = height() - topMargin - bottomMargin;
    int graphWidth = width() - leftMargin - rightMargin;
    
    double xStep = (double)graphWidth / (m_maxDataPoints - 1); 
    double yRange = m_maxY - m_minY;
    if(yRange == 0) yRange = 1;

    for (int i = 1; i < m_data.size(); ++i) {
        double x1 = leftMargin + (i - 1) * xStep; 
        double y1 = topMargin + graphHeight - ((m_data[i-1] - m_minY) / yRange * graphHeight);
        double x2 = leftMargin + i * xStep; 
        double y2 = topMargin + graphHeight - ((m_data[i] - m_minY) / yRange * graphHeight);
        p.drawLine(x1, y1, x2, y2);
    }
}

GraphWindow::GraphWindow(QWidget *parent) : QWidget(parent) {
    setWindowTitle("Grafikler");
    resize(600, 800);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background-color: #1a1b26; } QScrollBar:vertical { width: 10px; background: #16161e; } QScrollBar::handle:vertical { background: #3b4261; border-radius: 5px; }");
    
    m_contentWidget = new QWidget();
    m_contentWidget->setStyleSheet("background-color: #1a1b26;");
    m_layout = new QVBoxLayout(m_contentWidget);
    m_layout->setContentsMargins(10, 10, 10, 10);
    m_layout->setSpacing(10);
    m_layout->addStretch();
    
    scroll->setWidget(m_contentWidget);
    mainLayout->addWidget(scroll);
}

SimplePlot* GraphWindow::addPlot(const QString& title) {
    SimplePlot *plot = new SimplePlot(title, m_contentWidget);
    m_layout->insertWidget(m_layout->count() - 1, plot);
    return plot;
}

// --- Overlay Helper ---
void overlayResizeLogic(QWidget* parent, const QList<QWidget*>& overlays) {
    int padding = 20; 
    int y = parent->height() - padding;
    for(QWidget* w : overlays) { 
        w->adjustSize(); 
        int x = parent->width() - w->width() - padding; 
        y -= w->height();
        w->move(x, y); 
        y -= 10; 
    }
}

MapWidget::MapWidget(const QString &initialTheme, QWidget *parent) : QWidget(parent) {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

#ifdef USE_WEBENGINE
    m_webView = new QWebEngineView(this);
    m_isLoaded = false;
    
    QFile file(":/resources/map.html");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString html = QString::fromUtf8(file.readAll());
        html.replace("VAR_INITIAL_THEME", initialTheme);
        m_webView->setHtml(html, QUrl("qrc:/resources/"));
        file.close();
    }
    
    connect(m_webView, &QWebEngineView::loadFinished, this, &MapWidget::onLoadFinished);
    connect(m_webView, &QWebEngineView::urlChanged, this, &MapWidget::onUrlChanged);
    layout->addWidget(m_webView);
#else
    m_placeholder = new QLabel("WebEngine Devredisi\nTema: " + initialTheme, this);
    m_placeholder->setAlignment(Qt::AlignCenter);
    m_placeholder->setStyleSheet("color: white; font-size: 16px; background-color: #1a1b26;");
    layout->addWidget(m_placeholder);
#endif
    setLayout(layout);
}

#ifdef USE_WEBENGINE
void MapWidget::onLoadFinished(bool ok) {
    if (ok) {
        m_isLoaded = true;
        for (const QString &script : m_pendingScripts) m_webView->page()->runJavaScript(script);
        m_pendingScripts.clear();
    }
}

void MapWidget::onUrlChanged(const QUrl &url) {
    QString frag = url.fragment();
    if(frag.startsWith("addwp:")) {
        QStringList parts = frag.split(":"); 
        if(parts.size() >= 3) emit waypointClicked(parts[1].toDouble(), parts[2].toDouble());
    }
    else if(frag.startsWith("addzone:")) {
        QStringList parts = frag.split(":"); 
        if(parts.size() >= 3) emit zoneClicked(parts[1].toDouble(), parts[2].toDouble());
    }
    else if(frag.startsWith("copy:")) {
        QStringList parts = frag.split(":"); 
        if(parts.size() >= 3) QApplication::clipboard()->setText(parts[1] + ", " + parts[2]);
    }
    else if(frag.startsWith("theme:")) {
        QString themeName = frag.mid(6);
        themeName = QUrl::fromPercentEncoding(themeName.toUtf8());
        emit themeChanged(themeName);
    }
}

void MapWidget::runScript(const QString &script) {
    if (m_isLoaded) m_webView->page()->runJavaScript(script); 
    else m_pendingScripts.append(script);
}
#else
void MapWidget::onLoadFinished(bool) {}
void MapWidget::onUrlChanged(const QUrl &) {}
void MapWidget::runScript(const QString &) {}
#endif

void MapWidget::updatePosition(double lat, double lon, double heading) {
    if(lat != 0 && lon != 0) runScript(QString("updateDrone(%1, %2, %3);").arg(lat).arg(lon).arg(heading));
}

void MapWidget::drawMissionPath(const QList<Waypoint> &waypoints) {
    QJsonArray arr; 
    for(const auto& wp : waypoints) { 
        QJsonObject obj; obj["lat"] = wp.lat; obj["lon"] = wp.lon; arr.append(obj); 
    }
    runScript(QString("drawMission(%1);").arg(QString(QJsonDocument(arr).toJson(QJsonDocument::Compact))));
}

void MapWidget::drawMarkers(const QList<Zone> &zones, const QList<QPointF> &currentZone, const QList<CircleMarker> &circles) {
    QJsonObject root;
    QJsonArray zArr; 
    for(const auto& z : zones) { 
        QJsonArray pts; 
        for(const auto& p : z.points) { 
            QJsonArray pair; pair.append(p.x()); pair.append(p.y()); pts.append(pair); 
        } 
        zArr.append(pts); 
    } 
    root["zones"] = zArr;
    
    QJsonArray currArr; 
    for(const auto& p : currentZone) { 
        QJsonArray pair; pair.append(p.x()); pair.append(p.y()); currArr.append(pair); 
    } 
    root["currentZone"] = currArr;
    
    QJsonArray cArr; 
    for(const auto& c : circles) { 
        QJsonObject obj; obj["lat"] = c.lat; obj["lon"] = c.lon; obj["r"] = c.radius; cArr.append(obj); 
    } 
    root["circles"] = cArr;
    
    runScript(QString("drawMarkers(%1);").arg(QString(QJsonDocument(root).toJson(QJsonDocument::Compact))));
}

void MapWidget::setClickMode(bool enable) { runScript(QString("setClickMode('%1');").arg(enable ? "waypoint" : "none")); }
void MapWidget::setZoneMode(bool enable) { runScript(QString("setClickMode('%1');").arg(enable ? "zone" : "none")); }

void MapWidget::setBounds(double trLat, double trLon, double blLat, double blLon) {
    runScript(QString("if(typeof map !== 'undefined') { map.fitBounds([[%1, %2], [%3, %4]]); }")
              .arg(blLat).arg(blLon).arg(trLat).arg(trLon));
}

void MapWidget::setCenter(double lat, double lon) {
    runScript(QString("setMapCenter(%1, %2, 14);").arg(lat).arg(lon));
}

void MapWidget::addOverlayWidget(QWidget* w) { 
    w->setParent(this); 
    w->show(); 
    m_overlays.append(w); 
    overlayResizeLogic(this, m_overlays); 
}

void MapWidget::resizeEvent(QResizeEvent *e) { 
    QWidget::resizeEvent(e); 
    overlayResizeLogic(this, m_overlays); 
}