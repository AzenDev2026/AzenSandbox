// The Windows-11-style taskbar: a centred cluster (start, pinned, tasks)
// with the tray and clock on the right.
#pragma once

#include <QHash>
#include <QHBoxLayout>
#include <QIcon>
#include <QToolButton>
#include <QWidget>

class EmbeddedWindow;
class QLabel;
class QToolButton;
class StartMenu;

//: A task button with the little Win11 active underline.
class TaskButton : public QToolButton {
    Q_OBJECT
public:
    TaskButton(QWidget *parent);
    void setActiveUnderline(bool on);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    bool activeUnderline_ = false;
};

class Taskbar : public QWidget {
    Q_OBJECT
public:
    explicit Taskbar(QWidget *parent = nullptr);

    void addTask(EmbeddedWindow *w, const QIcon &icon);
    void removeTask(EmbeddedWindow *w);
    void refreshTask(EmbeddedWindow *w);
    void setActiveTask(EmbeddedWindow *w);
    void openStartMenu(); // used by the test mode too
    StartMenu *menuHandle() const { return menu_; }

signals:
    void appLaunchRequested(const QString &desktopId);
    void wineAppRequested(const QString &exe);
    void wineExeFileRequested();
    void shutdownRequested();
    void taskClicked(EmbeddedWindow *w);
    void fullscreenRequested();
    void borderlessRequested();

private:
    void updateClock();
    QToolButton *makeIconButton(const QIcon &icon, const QString &tooltip);

    StartMenu *menu_ = nullptr;
    QToolButton *startBtn_ = nullptr;
    QWidget *taskArea_ = nullptr;
    QHBoxLayout *taskLayout_ = nullptr;
    QHash<EmbeddedWindow *, TaskButton *> tasks_;
    QLabel *clock_ = nullptr;
};
