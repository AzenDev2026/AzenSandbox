// The sandbox itself: a toy desktop that spawns real programs and pulls
// their X windows inside, with a taskbar, start menu and Wine support.
#pragma once

#include <QAbstractNativeEventFilter>
#include <QHash>
#include <QIcon>
#include <QMainWindow>
#include <QSet>
#include <QTimer>
#include <xcb/xcb.h>
#include <xcb/damage.h>

class DesktopWidget;
class EmbeddedWindow;
class ProcessRunner;
class Taskbar;
class BootSplash;

class SandboxWindow : public QMainWindow, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit SandboxWindow();
    ~SandboxWindow() override;

    bool nativeEventFilter(const QByteArray &eventType, void *message,
                           qintptr *result) override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

public slots:
    void launchById(const QString &desktopId);
    void launchWineApp(const QString &exe);
    void openExeFile();
    void shutdownSandbox();
    void showToast(const QString &text);
    void toggleFullscreen();
    void toggleBorderless();

private:
    void poll();
    void tryEmbedAll();
    void spawn(const QString &program, const QStringList &args, const QString &icon,
               const QString &name);
    void tryEmbed(long pid);
    void embedWindow(xcb_window_t w, long pid);
    void activateWindow(EmbeddedWindow *w, bool focus = false);
    void removeWindow(EmbeddedWindow *w);
    void handleTaskClicked(EmbeddedWindow *w);
    void runTestMode();

    DesktopWidget *desktop_ = nullptr;
    Taskbar *taskbar_ = nullptr;
    BootSplash *splash_ = nullptr;
    QTimer *pollTimer_ = nullptr;

    QHash<qint64, ProcessRunner *> runners_;
    QHash<qint64, QIcon> pidIcons_;
    QHash<qint64, QString> pidNames_;
    QHash<qint64, qint64> spawnTimes_;   // msecs since epoch
    QHash<qint64, bool> pidHasWindow_;
    QSet<xcb_window_t> seenTopLevels_;
    QHash<xcb_window_t, EmbeddedWindow *> byXid_;
    QHash<xcb_window_t, xcb_damage_damage_t> damagesByXid_;
    QHash<EmbeddedWindow *, qint64> windowPid_;
    EmbeddedWindow *active_ = nullptr;
    int cascade_ = 0;
    bool testMode_ = false;
    bool shuttingDown_ = false;
    bool borderless_ = false;
};
