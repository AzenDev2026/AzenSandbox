#include "taskbar.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QToolButton>

#include "embedded.h"
#include "startmenu.h"
#include "theme.h"

Taskbar::Taskbar(QWidget *parent) : QWidget(parent) {
    setObjectName("taskbar");
    setFixedHeight(48);
    setStyleSheet(QStringLiteral(
        "#taskbar { background: %1; border-top: 1px solid %2; }"
        "QToolButton { border: none; background: transparent; color: %3; border-radius: 6px; }"
        "QToolButton:hover { background: rgba(255,255,255,0.09); }"
        "QToolButton:checked { background: rgba(255,255,255,0.16); }"
        "#startButton { font-weight: bold; padding: 0 12px; font-size: 13px; }"
        "#taskButton { padding: 4px 10px; max-width: 180px; font-size: 12px; }"
        "QLabel { color: %3; font-size: 12px; }")
        .arg(gTheme.panelBg.name(), gTheme.border.name(), gTheme.text.name()));

    auto *bar = new QHBoxLayout(this);
    bar->setContentsMargins(6, 0, 8, 0);
    bar->setSpacing(4);

    startBtn_ = new QToolButton(this);
    startBtn_->setObjectName("startButton");
    startBtn_->setText(QStringLiteral("🐳 Azen"));
    startBtn_->setCursor(Qt::PointingHandCursor);
    connect(startBtn_, &QToolButton::clicked, this, &Taskbar::showStartMenu);
    bar->addWidget(startBtn_);

    // Pinned launchers (duplicated here for the "real panel" feel).
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
    bar->addSpacing(6);

    bar->addStretch(1);

    // Toy tray.
    bar->addWidget(makeIconButton(QIcon::fromTheme(QStringLiteral("audio-volume-high")),
                                  QStringLiteral("Sound (toy)")));
    bar->addWidget(makeIconButton(QIcon::fromTheme(QStringLiteral("network-wireless")),
                                  QStringLiteral("Network (toy)")));
    bar->addWidget(makeIconButton(QIcon::fromTheme(QStringLiteral("battery-full")),
                                  QStringLiteral("Battery (toy)")));

    clock_ = new QLabel(this);
    clock_->setObjectName("clock");
    clock_->setAlignment(Qt::AlignCenter);
    clock_->setMinimumWidth(64);
    bar->addWidget(clock_);

    auto *timer = new QTimer(this);
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, &Taskbar::updateClock);
    timer->start();
    updateClock();
}

QToolButton *Taskbar::makeIconButton(const QIcon &icon, const QString &tooltip) {
    auto *b = new QToolButton(this);
    b->setIcon(icon);
    b->setIconSize(QSize(20, 20));
    b->setFixedSize(34, 34);
    b->setToolTip(tooltip);
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}

void Taskbar::showStartMenu() {
    if (!menu_) {
        menu_ = new StartMenu(this);
        connect(menu_, &StartMenu::appLaunchRequested, this, &Taskbar::appLaunchRequested);
        connect(menu_, &StartMenu::wineAppRequested, this, &Taskbar::wineAppRequested);
        connect(menu_, &StartMenu::wineExeFileRequested, this, &Taskbar::wineExeFileRequested);
        connect(menu_, &StartMenu::shutdownRequested, this, &Taskbar::shutdownRequested);

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
    const QPoint buttonBottom = startBtn_->mapToGlobal(QPoint(0, startBtn_->height()));
    const QPoint pos(buttonBottom.x(), buttonBottom.y() - menu_->sizeHint().height() - 4);
    menu_->popupAt(pos);
}

void Taskbar::addTask(EmbeddedWindow *w, const QIcon &icon) {
    auto *b = new QToolButton(this);
    b->setObjectName("taskButton");
    b->setCheckable(true);
    b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    b->setIcon(icon);
    b->setIconSize(QSize(20, 20));
    b->setText(w->title().isEmpty() ? QStringLiteral("Window") : w->title());
    b->setToolTip(w->title());
    connect(b, &QToolButton::clicked, this, [this, w]() { emit taskClicked(w); });
    auto *layout = static_cast<QHBoxLayout *>(this->layout());
    layout->insertWidget(5, b);
    tasks_.insert(w, b);
}

void Taskbar::removeTask(EmbeddedWindow *w) {
    QToolButton *b = tasks_.take(w);
    if (b)
        b->deleteLater();
}

void Taskbar::refreshTask(EmbeddedWindow *w) {
    QToolButton *b = tasks_.value(w);
    if (!b)
        return;
    b->setText(w->title().isEmpty() ? QStringLiteral("Window") : w->title());
    b->setChecked(!w->isMinimized());
}

void Taskbar::setActiveTask(EmbeddedWindow *w) {
    for (auto it = tasks_.constBegin(); it != tasks_.constEnd(); ++it)
        it.value()->setChecked(it.key() == w && !it.key()->isMinimized());
}

void Taskbar::updateClock() {
    const QDateTime now = QDateTime::currentDateTime();
    clock_->setText(now.toString(QStringLiteral("HH:mm")));
    clock_->setToolTip(now.toString(QStringLiteral("yyyy-MM-dd dddd")));
}
