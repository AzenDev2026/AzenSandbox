// The bottom panel: start button, pinned apps, task buttons for every
// embedded window, a toy tray and the clock — modelled on the user's
// Plasma panel (48 px, solid #202020).
#pragma once

#include <QHash>
#include <QIcon>
#include <QWidget>

class EmbeddedWindow;
class QLabel;
class QToolButton;
class StartMenu;

class Taskbar : public QWidget {
    Q_OBJECT
public:
    explicit Taskbar(QWidget *parent = nullptr);

    void addTask(EmbeddedWindow *w, const QIcon &icon);
    void removeTask(EmbeddedWindow *w);
    void refreshTask(EmbeddedWindow *w);
    void setActiveTask(EmbeddedWindow *w);

signals:
    void appLaunchRequested(const QString &desktopId);
    void wineAppRequested(const QString &exe);
    void wineExeFileRequested();
    void shutdownRequested();
    void taskClicked(EmbeddedWindow *w);

private:
    void updateClock();
    void showStartMenu();
    QToolButton *makeIconButton(const QIcon &icon, const QString &tooltip);

    StartMenu *menu_ = nullptr;
    QToolButton *startBtn_ = nullptr;
    QHash<EmbeddedWindow *, QToolButton *> tasks_;
    QLabel *clock_ = nullptr;
};
