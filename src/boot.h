// The 5-second boot splash: 2 seconds of streaming boot/code output with the
// NVazen Linux logo, then a 3-second macOS-style progress bar, then a fade
// into the desktop.
#pragma once

#include <QElapsedTimer>
#include <QPixmap>
#include <QStringList>
#include <QTimer>
#include <QWidget>

class BootSplash : public QWidget {
    Q_OBJECT
public:
    explicit BootSplash(QWidget *parent = nullptr);

signals:
    void finished();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void tick();
    void addLogLines(int count);
    static QString randomHexDump();

    QElapsedTimer clock_;
    QPixmap logo_;
    QStringList logLines_;
    QStringList logPool_;
    QTimer *timer_ = nullptr;
    double progress_ = 0.0;   // 0..1 during phase 2
    double fade_ = 1.0;       // fade-out at the end
    bool finishing_ = false;
};
