#include "taskbar.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QToolButton>

#include "embedded.h"
#include "startmenu.h"
#include "theme.h"

// ------------------------------------------------------------ task button --

TaskButton::TaskButton(QWidget *parent) : QToolButton(parent) {
    setObjectName("taskButton");
    setCheckable(false);
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    setIconSize(QSize(20, 20));
    setFixedHeight(40);
    setMaximumWidth(190);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setMouseTracking(true);
}

void TaskButton::setActiveUnderline(bool on) {
    if (activeUnderline_ == on)
        return;
    activeUnderline_ = on;
    update();
}

void TaskButton::paintEvent(QPaintEvent *event) {
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        if (underMouse() || isDown()) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255, isDown() ? 22 : 14));
            p.drawRoundedRect(rect().adjusted(2, 3, -2, -3), 4, 4);
        }
    }

    QToolButton::paintEvent(event); // icon + label (creates its own painter)

    if (activeUnderline_) {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(gTheme.accentHover);
        p.drawRoundedRect(QRect(width() / 2 - 8, height() - 5, 16, 3), 1.5, 1.5);
    }
}

// ------------------------------------------------------------------ panel --

Taskbar::Taskbar(QWidget *parent) : QWidget(parent) {
    setObjectName("taskbar");
    setFixedHeight(48);
    setStyleSheet(QStringLiteral(
        "#taskbar { background: %1; border-top: 1px solid rgba(255,255,255,0.06); }"
        "QToolButton { border: none; background: transparent; color: %2; border-radius: 6px; }"
        "QToolButton:hover { background: rgba(255,255,255,0.08); }"
        "#startButton { border-radius: 6px; }"
        "#taskButton { padding: 0 10px; font-size: 12px; color: %2; }"
        "QLabel { color: %2; font-size: 12px; background: transparent; }")
        .arg(gTheme.panelBg.name(), gTheme.text.name()));

    auto *bar = new QHBoxLayout(this);
    bar->setContentsMargins(8, 4, 8, 4);
    bar->setSpacing(6);

    // Left stretch — the cluster below sits centred between two stretches.
    bar->addStretch(1);

    startBtn_ = new QToolButton(this);
    startBtn_->setObjectName("startButton");
    startBtn_->setIcon(QIcon(QStringLiteral(":/assets/azen-sandbox.png")));
    startBtn_->setIconSize(QSize(26, 26));
    startBtn_->setFixedSize(40, 40);
    startBtn_->setToolTip(QStringLiteral("Start"));
    startBtn_->setCursor(Qt::PointingHandCursor);
    connect(startBtn_, &QToolButton::clicked, this, &Taskbar::openStartMenu);
    bar->addWidget(startBtn_);

    auto pinned = [this](const QIcon &icon, const QString &tooltip, const QString &id) {
        QToolButton *b = makeIconButton(icon, tooltip);
        connect(b, &QToolButton::clicked, this, [this, id]() { emit appLaunchRequested(id); });
        return b;
    };
    bar->addWidget(pinned(QIcon::fromTheme(QStringLiteral("utilities-terminal")),
                          QStringLiteral("Terminal"), QStringLiteral("org.kde.konsole")));
    bar->addWidget(pinned(QIcon::fromTheme(QStringLiteral("system-file-manager")),
                          QStringLiteral("Files"), QStringLiteral("org.kde.dolphin")));
    bar->addWidget(pinned(QIcon::fromTheme(QStringLiteral("firefox")),
                          QStringLiteral("Firefox"), QStringLiteral("firefox")));

    taskArea_ = new QWidget(this);
    taskLayout_ = new QHBoxLayout(taskArea_);
    taskLayout_->setContentsMargins(0, 0, 0, 0);
    taskLayout_->setSpacing(6);
    bar->addWidget(taskArea_);

    bar->addStretch(1);

    // Tray + clock.
    bar->addWidget(makeIconButton(QIcon::fromTheme(QStringLiteral("audio-volume-high")),
                                  QStringLiteral("Sound (toy)")));
    bar->addWidget(makeIconButton(QIcon::fromTheme(QStringLiteral("network-wireless")),
                                  QStringLiteral("Network (toy)")));
    bar->addWidget(makeIconButton(QIcon::fromTheme(QStringLiteral("battery-full")),
                                  QStringLiteral("Battery (toy)")));

    clock_ = new QLabel(this);
    clock_->setAlignment(Qt::AlignCenter);
    clock_->setMinimumWidth(58);
    bar->addWidget(clock_);

    auto *timer = new QTimer(this);
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, &Taskbar::updateClock);
    timer->start();
    updateClock();

    if (qEnvironmentVariableIsSet("AZS_DEBUG_TASKBAR")) {
        QTimer::singleShot(1500, this, [this]() {
            const QPoint g = startBtn_->mapToGlobal(QPoint(0, 0));
            fprintf(stderr,
                    "TASKBAR_DEBUG taskbar_y=%d taskbar_h=%d start_global=(%d,%d) start_size=(%d,%d) "
                    "enabled=%d visible=%d underMouse=%d\n",
                    mapToGlobal(QPoint(0, 0)).y(), height(), g.x(), g.y(),
                    startBtn_->width(), startBtn_->height(), startBtn_->isEnabled(),
                    startBtn_->isVisible(), startBtn_->underMouse());
        });
    }
}

QToolButton *Taskbar::makeIconButton(const QIcon &icon, const QString &tooltip) {
    auto *b = new QToolButton(this);
    b->setIcon(icon);
    b->setIconSize(QSize(18, 18));
    b->setFixedSize(36, 36);
    b->setToolTip(tooltip);
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}

void Taskbar::openStartMenu() {
    if (!menu_) {
        menu_ = new StartMenu(this);
        connect(menu_, &StartMenu::appLaunchRequested, this, &Taskbar::appLaunchRequested);
        connect(menu_, &StartMenu::wineAppRequested, this, &Taskbar::wineAppRequested);
        connect(menu_, &StartMenu::wineExeFileRequested, this, &Taskbar::wineExeFileRequested);
        connect(menu_, &StartMenu::shutdownRequested, this, &Taskbar::shutdownRequested);
        connect(menu_, &StartMenu::fullscreenRequested, this, &Taskbar::fullscreenRequested);
        connect(menu_, &StartMenu::borderlessRequested, this, &Taskbar::borderlessRequested);

        QString wine = QStandardPaths::findExecutable(QStringLiteral("wine"));
        if (wine.isEmpty())
            wine = QStandardPaths::findExecutable(QStringLiteral("wine64"));
        if (!wine.isEmpty()) {
            QProcess probe;
            probe.start(wine, {QStringLiteral("--version")});
            probe.waitForFinished(3000);
            QString version = QString::fromLocal8Bit(probe.readAllStandardOutput()).trimmed();
            if (version.startsWith(QLatin1String("wine-"), Qt::CaseInsensitive))
                version = version.mid(5);
            menu_->setWineAvailable(true, version.isEmpty() ? QStringLiteral("detected") : version);
        } else {
            menu_->setWineAvailable(false, QString());
        }
    }
    // Fixed position: centred horizontally, sitting just above the taskbar.
    QWidget *root = window();
    if (!root)
        root = this;
    const QPoint rootCenter =
        root->mapToGlobal(QPoint(root->width() / 2, 0));
    const int taskbarY = mapToGlobal(QPoint(0, 0)).y();
    const int menuY = taskbarY - menu_->height() - 10;
    if (qEnvironmentVariableIsSet("AZS_DEBUG_MENU"))
        fprintf(stderr, "MENU_DEBUG rootCenter=%d taskbarY=%d menuY=%d menu_h=%d\n",
                rootCenter.x(), taskbarY, menuY, menu_->height());
    menu_->popupAt(QPoint(rootCenter.x() - menu_->width() / 2, menuY));
}

void Taskbar::addTask(EmbeddedWindow *w, const QIcon &icon) {
    auto *b = new TaskButton(taskArea_);
    b->setIcon(icon);
    b->setText(w->title().isEmpty() ? QStringLiteral("Window") : w->title());
    b->setToolTip(w->title());
    connect(b, &QToolButton::clicked, this, [this, w]() { emit taskClicked(w); });
    taskLayout_->addWidget(b);
    tasks_.insert(w, b);
}

void Taskbar::removeTask(EmbeddedWindow *w) {
    TaskButton *b = tasks_.take(w);
    if (b)
        b->deleteLater();
}

void Taskbar::refreshTask(EmbeddedWindow *w) {
    TaskButton *b = tasks_.value(w);
    if (!b)
        return;
    b->setText(w->title().isEmpty() ? QStringLiteral("Window") : w->title());
    b->setToolTip(w->title());
    b->setActiveUnderline(!w->isMinimized() && w->isActive());
}

void Taskbar::setActiveTask(EmbeddedWindow *w) {
    for (auto it = tasks_.constBegin(); it != tasks_.constEnd(); ++it)
        it.value()->setActiveUnderline(it.key() == w && !it.key()->isMinimized());
}

void Taskbar::updateClock() {
    const QDateTime now = QDateTime::currentDateTime();
    clock_->setText(now.toString(QStringLiteral("HH:mm")));
    clock_->setToolTip(now.toString(QStringLiteral("yyyy-MM-dd dddd")));
}
