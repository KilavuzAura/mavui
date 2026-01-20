#include "logviewer.h"
#include "logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMessageBox>
#include <QScrollBar>

LogViewer::LogViewer(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Log Goruntuleyici");
    resize(700, 500);

    QVBoxLayout *layout = new QVBoxLayout(this);

    m_tabs = new QTabWidget(this);
    
    auto createTab = [&](const QString &title) -> QTextEdit* {
        QTextEdit *txt = new QTextEdit();
        txt->setReadOnly(true);
        txt->setStyleSheet("background-color: #1a1b26; color: #a9b1d6; font-family: Monospace; font-size: 11px; border: none;");
        m_tabs->addTab(txt, title);
        return txt;
    };

    m_txtInfo = createTab("Info (Bilgi)");
    m_txtWarning = createTab("Warning (Uyari)");
    m_txtError = createTab("Error (Hata)");
    m_txtDebug = createTab("Debug (Hata Ayiklama)");
    m_txtFeatures = createTab("Features / Diger");

    layout->addWidget(m_tabs);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    
    QPushButton *btnRefresh = new QPushButton("Yenile");
    btnRefresh->setStyleSheet("background-color: #3b82f6; color: white; font-weight: bold; padding: 6px;");
    connect(btnRefresh, &QPushButton::clicked, this, &LogViewer::refreshLogs);
    
    QPushButton *btnClear = new QPushButton("Secili Logu Temizle");
    btnClear->setStyleSheet("background-color: #f7768e; color: white; font-weight: bold; padding: 6px;");
    connect(btnClear, &QPushButton::clicked, this, &LogViewer::clearCurrentLog);
    
    QPushButton *btnClose = new QPushButton("Kapat");
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);

    btnLayout->addWidget(btnRefresh);
    btnLayout->addWidget(btnClear);
    btnLayout->addStretch();
    btnLayout->addWidget(btnClose);

    layout->addLayout(btnLayout);

    refreshLogs();
}

void LogViewer::refreshLogs() {
    m_txtInfo->setText(Logger::readLog(Logger::Info));
    m_txtWarning->setText(Logger::readLog(Logger::Warning));
    m_txtError->setText(Logger::readLog(Logger::Error));
    m_txtDebug->setText(Logger::readLog(Logger::Debug));
    m_txtFeatures->setText(Logger::readLog(Logger::Features));
    
    auto scrollToBottom = [](QTextEdit* txt){
        txt->verticalScrollBar()->setValue(txt->verticalScrollBar()->maximum());
    };
    
    scrollToBottom(m_txtInfo);
    scrollToBottom(m_txtWarning);
    scrollToBottom(m_txtError);
    scrollToBottom(m_txtDebug);
    scrollToBottom(m_txtFeatures);
}

void LogViewer::clearCurrentLog() {
    int index = m_tabs->currentIndex();
    Logger::LogLevel level;
    QTextEdit *target = nullptr;

    if (m_tabs->currentWidget() == m_txtInfo) { level = Logger::Info; target = m_txtInfo; }
    else if (m_tabs->currentWidget() == m_txtWarning) { level = Logger::Warning; target = m_txtWarning; }
    else if (m_tabs->currentWidget() == m_txtError) { level = Logger::Error; target = m_txtError; }
    else if (m_tabs->currentWidget() == m_txtDebug) { level = Logger::Debug; target = m_txtDebug; }
    else if (m_tabs->currentWidget() == m_txtFeatures) { level = Logger::Features; target = m_txtFeatures; }
    else return;

    if(QMessageBox::question(this, "Temizle", "Bu log dosyasini silmek istediginizden emin misiniz?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        Logger::clearLog(level);
        target->clear();
    }
}