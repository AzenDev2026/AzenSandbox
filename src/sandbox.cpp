#include "sandbox.h"

#include <QCloseEvent>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>
#include <unistd.h>

#include "apps.h"
#include "boot.h"
#include "desktop.h"
#include "embedded.h"
#include "proc.h"
#include "taskbar.h"
#include "theme.h"
#include "x11.h"

namespace {
constexpr int POLL_MS = 180;

QString wineExecutable() {
    QString wine = QStandardPaths::findExecutable(QStringLiteral("wine"));
    if (wine.isEmpty())
        wine = QStandardPaths::findExecutable(QStringLiteral("wine64"));
    return wine;
}
} // namespace

SandboxWindow::SandboxWindow() {
    setWindowTitle(QStringLiteral("Azen Sandbox"));
    resize(1280, 780);
    setMinimumSize(720, 480);
    testMode_ = qEnvironmentVariableIsSet("AZS_TEST");

    auto *central = new QWidget(this);
    auto *vbox = new QVBoxLayout(central);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);
    desktop_ = new DesktopWidget(central);
    taskbar_ = new Taskbar(central);
    vbox->addWidget(desktop_, 1);
    vbox->addWidget(taskbar_);
    setCentralWidget(central);

    QCoreApplication::instance()->installNativeEventFilter(this);

    connect(desktop_, &DesktopWidget::launchRequested, this, &SandboxWindow::launchById);
    connect(taskbar_, &Taskbar::appLaunchRequested, this, &SandboxWindow::launchById);
    connect(taskbar_, &Taskbar::wineAppRequested, this, &SandboxWindow::launchWineApp);
    connect(taskbar_, &Taskbar::wineExeFileRequested, this, &SandboxWindow::openExeFile);
    connect(taskbar_, &Taskbar::shutdownRequested, this, &SandboxWindow::shutdownSandbox);
    connect(taskbar_, &Taskbar::taskClicked, this, &SandboxWindow::handleTaskClicked);

    pollTimer_ = new QTimer(this);
    pollTimer_->setInterval(POLL_MS);
    connect(pollTimer_, &QTimer::timeout, this, &SandboxWindow::poll);
    pollTimer_->start();

    if (testMode_)
        QTimer::singleShot(400, this, &SandboxWindow::runTestMode);

    // 5-second boot animation: 2 s of code output + logo, 3 s progress bar.
    if (!testMode_ && !qEnvironmentVariableIsSet("AZS_NO_BOOT")) {
        splash_ = new BootSplash(central);
        splash_->setGeometry(central->rect());
        splash_->show();
        splash_->raise();
        connect(splash_, &BootSplash::finished, splash_, &QObject::deleteLater);
        connect(splash_, &BootSplash::finished, this, [this]() {
            showToast(QStringLiteral("🐳 Welcome to Azen Sandbox — open the start menu!"));
        });
        connect(splash_, &QObject::destroyed, this, [this]() { splash_ = nullptr; });
    } else {
        showToast(QStringLiteral("🐳 Welcome to Azen Sandbox — open the start menu and launch something!"));
    }
}

void SandboxWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    if (splash_ && centralWidget())
        splash_->setGeometry(centralWidget()->rect());
}

SandboxWindow::~SandboxWindow() {
    shuttingDown_ = true;
    // Stop the runners before the child QObjects are deleted, and detach
    // their signals: during QWidget teardown QProcess may still emit
    // finished/exited while our member hashes are already gone.
    const auto runners = runners_.values();
    for (ProcessRunner *r : runners) {
        r->terminate(150);
        disconnect(r, nullptr, this, nullptr);
    }
}

// -- spawning ----------------------------------------------------------

void SandboxWindow::spawn(const QString &program, const QStringList &args, const QString &icon,
                          const QString &name) {
    auto *runner = new ProcessRunner(program, args, this);
    connect(runner, &ProcessRunner::started, this, [this, runner, icon, name](qint64 pid) {
        pidIcons_.insert(pid, QIcon::fromTheme(icon));
        pidNames_.insert(pid, name);
        spawnTimes_.insert(pid, QDateTime::currentMSecsSinceEpoch());
        pidHasWindow_.insert(pid, false);
        runners_.insert(pid, runner);
    });
    connect(runner, &ProcessRunner::exited, this, [this](qint64 pid) {
        runners_.remove(pid);
        // Kill the frames whose process is gone.
        const auto wins = byXid_.values();
        for (EmbeddedWindow *w : wins) {
            if (windowPid_.value(w) == pid)
                removeWindow(w);
        }
    });
    runner->start();
    showToast(QStringLiteral("▶ %1").arg(name));
}

void SandboxWindow::launchById(const QString &desktopId) {
    const QVector<DesktopApp> apps = AppList::load();
    for (const DesktopApp &app : apps) {
        if (app.id != desktopId && app.id != desktopId + QStringLiteral(".desktop"))
            continue;
        const QStringList args = app.launchArgs();
        if (args.isEmpty()) {
            showToast(QStringLiteral("Cannot launch %1: no command").arg(app.name));
            return;
        }
        spawn(args.first(), args.mid(1), app.icon, app.name);
        return;
    }
    showToast(QStringLiteral("App not found: %1").arg(desktopId));
}

void SandboxWindow::launchWineApp(const QString &exe) {
    const QString wine = wineExecutable();
    if (wine.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Wine"),
                                 QStringLiteral(
                                     "Wine is not installed, so Windows apps cannot run here yet.\n\n"
                                     "Install it with:  sudo pacman -S wine\n"
                                     "then restart Azen Sandbox."));
        return;
    }
    spawn(wine, {exe}, QStringLiteral("wine"),
          QStringLiteral("Wine · %1").arg(exe));
}

void SandboxWindow::openExeFile() {
    const QString wine = wineExecutable();
    if (wine.isEmpty()) {
        launchWineApp(QStringLiteral("notepad")); // shows the install hint
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Open a Windows program"),
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
        QStringLiteral("Windows programs (*.exe *.msi)"));
    if (path.isEmpty())
        return;
    spawn(wine, {path}, QStringLiteral("wine"),
          QStringLiteral("Wine · %1").arg(QFileInfo(path).fileName()));
}

// -- window discovery --------------------------------------------------

void SandboxWindow::poll() {
    if (shuttingDown_)
        return;
    // Try to adopt windows of every running process.
    const QList<qint64> pids = runners_.keys();
    for (qint64 pid : pids)
        tryEmbed(pid);

    // Refresh titles (cheap) and prune dead frames.
    for (auto it = byXid_.begin(); it != byXid_.end();) {
        if (!X11::windowAlive(it.key())) {
            EmbeddedWindow *w = it.value();
            it = byXid_.erase(it);
            removeWindow(w);
            continue;
        }
        const QString title = X11::windowTitle(it.key());
        EmbeddedWindow *w = it.value();
        if (!title.isEmpty() && title != w->title()) {
            // update the label via the taskbar refresh: store in window data
            w->setWindowTitleText(title);
            taskbar_->refreshTask(w);
        }
        ++it;
    }
}

void SandboxWindow::tryEmbed(long pid) {
    const QVector<xcb_window_t> tops = X11::topLevelWindows();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 spawnAge = now - spawnTimes_.value(pid, now);
    const bool needsWindow = !pidHasWindow_.value(pid, false) && spawnAge < 9000;

    for (xcb_window_t w : tops) {
        if (byXid_.contains(w))
            continue;
        if (w == static_cast<xcb_window_t>(winId()))
            continue;
        const bool isNew = !seenTopLevels_.contains(w);
        long windowPid = -1;
        const bool hasPid = X11::windowPid(w, &windowPid) && windowPid > 0;
        if (hasPid) {
            // Skip our own helper windows (menus, popups).
            if (windowPid == static_cast<long>(getpid()))
                continue;
            // Match either the exact pid or anything in the spawned process
            // group (wine and multi-process apps fork their windows).
            if (windowPid != pid && X11::processGroupOf(windowPid) != pid)
                continue;
        } else if (needsWindow && isNew && X11::hasWmClass(w)) {
            // Legacy apps (xmessage, some wine windows) set no _NET_WM_PID:
            // adopt a freshly appeared window for a spawn that has none yet.
        } else {
            continue;
        }
        embedWindow(w, pid);
    }
    // Remember everything observed, including windows we skipped.
    for (xcb_window_t w : tops)
        seenTopLevels_.insert(w);
}

void SandboxWindow::embedWindow(xcb_window_t w, long pid) {
    pidHasWindow_[pid] = true;
    const QString title = X11::windowTitle(w);
    auto *ew = new EmbeddedWindow(title.isEmpty() ? pidNames_.value(pid) : title, desktop_);
    ew->setTitleIcon(pidIcons_.value(pid));
    ew->resize(560, 400);
    const int step = (cascade_ % 7) * 34;
    ew->move(70 + step, 46 + step);
    connect(ew, &EmbeddedWindow::closeRequested, this, [this](EmbeddedWindow *win) {
        const qint64 pid = windowPid_.value(win, -1);
        X11::requestClose(win->xid());
        if (pid > 0 && runners_.contains(pid)) {
            QTimer::singleShot(1500, this, [this, win, pid]() {
                if (X11::windowAlive(win->xid()) && runners_.contains(pid))
                    runners_.value(pid)->terminate();
            });
        }
    });
    connect(ew, &EmbeddedWindow::stateChanged, this,
            [this](EmbeddedWindow *win) { taskbar_->refreshTask(win); });
    byXid_.insert(w, ew);
    windowPid_.insert(ew, pid);
    ew->show();
    ew->attach(w);
    taskbar_->addTask(ew, pidIcons_.value(pid));
    activateWindow(ew);
    ++cascade_;
    showToast(QStringLiteral("▣ %1 joined the sandbox").arg(ew->title()));
}

void SandboxWindow::activateWindow(EmbeddedWindow *w, bool focus) {
    if (w->isMinimized())
        w->restore();
    if (active_ && active_ != w)
        active_->setActive(false);
    active_ = w;
    w->setActive(true);
    w->raise();
    X11::raiseWindow(w->xid());
    if (focus)
        X11::setInputFocus(w->xid());
    X11::flush();
    taskbar_->setActiveTask(w);
}

void SandboxWindow::removeWindow(EmbeddedWindow *w) {
    byXid_.remove(w->xid());
    windowPid_.remove(w);
    if (active_ == w)
        active_ = nullptr;
    taskbar_->removeTask(w);
    w->deleteLater();
}

void SandboxWindow::handleTaskClicked(EmbeddedWindow *w) {
    if (w->isMinimized()) {
        activateWindow(w, true);
    } else if (w->isActive()) {
        w->minimize();
        taskbar_->setActiveTask(w);
    } else {
        activateWindow(w, true);
    }
}

// -- xcb events --------------------------------------------------------

bool SandboxWindow::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *) {
    if (shuttingDown_)
        return false;
    if (eventType != "xcb_generic_event_t")
        return false;
    auto *event = static_cast<xcb_generic_event_t *>(message);
    const uint8_t type = event->response_type & ~0x80;

    switch (type) {
    case XCB_BUTTON_PRESS: {
        auto *press = reinterpret_cast<xcb_button_press_event_t *>(event);
        EmbeddedWindow *w = byXid_.value(press->event, nullptr);
        if (w) {
            X11::setInputFocus(press->event);
            X11::raiseWindow(press->event);
            activateWindow(w);
        }
        break;
    }
    case XCB_FOCUS_IN: {
        auto *focus = reinterpret_cast<xcb_focus_in_event_t *>(event);
        EmbeddedWindow *w = byXid_.value(focus->event, nullptr);
        if (w)
            activateWindow(w);
        break;
    }
    case XCB_DESTROY_NOTIFY: {
        auto *destroy = reinterpret_cast<xcb_destroy_notify_event_t *>(event);
        EmbeddedWindow *w = byXid_.value(destroy->window, nullptr);
        if (w)
            removeWindow(w);
        break;
    }
    case XCB_CONFIGURE_NOTIFY: {
        auto *cfg = reinterpret_cast<xcb_configure_notify_event_t *>(event);
        EmbeddedWindow *w = byXid_.value(cfg->window, nullptr);
        if (w && cfg->width > 1 && cfg->height > 1) {
            // The app resized its own window: follow it, keep it at 0,0.
            const int frameW = static_cast<int>(cfg->width);
            const int frameH = static_cast<int>(cfg->height) + EmbeddedWindow::TITLEBAR_H;
            if (w->size() != QSize(frameW, frameH) && !w->isMaximized()) {
                w->resize(frameW, frameH);
                X11::moveResize(cfg->window, 0, 0, cfg->width, cfg->height);
                X11::flush();
            }
        }
        break;
    }
    case XCB_PROPERTY_NOTIFY: {
        auto *prop = reinterpret_cast<xcb_property_notify_event_t *>(event);
        EmbeddedWindow *w = byXid_.value(prop->window, nullptr);
        if (w) {
            const QString title = X11::windowTitle(prop->window);
            if (!title.isEmpty() && title != w->title()) {
                w->setWindowTitleText(title);
                taskbar_->refreshTask(w);
            }
        }
        break;
    }
    default:
        break;
    }
    return false;
}

// -- lifecycle ---------------------------------------------------------

void SandboxWindow::shutdownSandbox() {
    close();
}

void SandboxWindow::closeEvent(QCloseEvent *event) {
    const auto runners = runners_.values();
    for (ProcessRunner *r : runners)
        r->terminate(1200);
    QMainWindow::closeEvent(event);
}

void SandboxWindow::showToast(const QString &text) {
    auto *label = new QLabel(text, desktop_);
    label->setObjectName("toast");
    label->setStyleSheet(QStringLiteral(
        "QLabel#toast { background: rgba(20,20,20,0.92); color: white; border-radius: 16px;"
        "  padding: 8px 18px; font-size: 13px; }"));
    label->adjustSize();
    label->move((desktop_->width() - label->width()) / 2, desktop_->height() - 90);
    label->show();
    label->raise();
    QTimer::singleShot(3800, label, &QObject::deleteLater);
}

// -- test mode ---------------------------------------------------------

void SandboxWindow::runTestMode() {
    // Launch a couple of guaranteed-simple X11 apps and report what got
    // embedded, so the build can be verified automatically.
    const QString xmessage = QStandardPaths::findExecutable(QStringLiteral("xmessage"));
    if (!xmessage.isEmpty())
        spawn(xmessage, {QStringLiteral("Hello from Azen Sandbox!")},
              QStringLiteral("dialog-information"), QStringLiteral("xmessage"));
    const QString konsole = QStandardPaths::findExecutable(QStringLiteral("konsole"));
    if (!konsole.isEmpty())
        spawn(konsole, {}, QStringLiteral("utilities-terminal"), QStringLiteral("Konsole"));

    QTimer::singleShot(5000, this, [this]() {
        QJsonArray wins;
        for (auto it = byXid_.constBegin(); it != byXid_.constEnd(); ++it) {
            QJsonObject o;
            o.insert("title", it.value()->title());
            o.insert("minimized", it.value()->isMinimized());
            o.insert("x", it.value()->x());
            o.insert("y", it.value()->y());
            o.insert("w", it.value()->width());
            o.insert("h", it.value()->height());
            wins.append(o);
        }
        QJsonObject report;
        report.insert("embedded", static_cast<int>(byXid_.size()));
        report.insert("windows", wins);
        fprintf(stdout, "AZS_TEST_REPORT %s\n",
                QJsonDocument(report).toJson(QJsonDocument::Compact).constData());
        fflush(stdout);

        // Screenshot for visual verification.
        QString shotPath = qEnvironmentVariable("AZS_TEST_SHOT");
        if (shotPath.isEmpty())
            shotPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) +
                       "/Desktop/AzenSandbox/test-shot.png";
        if (QScreen *screen = windowHandle() ? windowHandle()->screen() : nullptr) {
            screen->grabWindow(winId()).save(shotPath);
            fprintf(stdout, "AZS_TEST_SHOT %s\n", shotPath.toUtf8().constData());
            fflush(stdout);
        }
        QCoreApplication::quit();
    });
}
