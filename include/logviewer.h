#ifndef LOGVIEWER_H
#define LOGVIEWER_H

#include <QDialog>
#include <QTextEdit>
#include <QTabWidget>

class LogViewer : public QDialog {
    Q_OBJECT

public:
    explicit LogViewer(QWidget *parent = nullptr);

public slots:
    void refreshLogs();

private slots:
    void clearCurrentLog();

private:
    QTabWidget *m_tabs;
    
    QTextEdit *m_txtInfo;
    QTextEdit *m_txtWarning;
    QTextEdit *m_txtError;
    QTextEdit *m_txtDebug;
    QTextEdit *m_txtFeatures;
};

#endif // LOGVIEWER_H