// A window frame drawn by the sandbox with a foreign X window reparented
// into the content area: move, minimize, maximize and close, toy-style.
#pragma once

#include <QWidget>
#include <xcb/xcb.h>

class QLabel;
class QPushButton;

class EmbeddedWindow : public QWidget {
    Q_OBJECT
public:
    explicit EmbeddedWindow(const QString &title, QWidget *parent);
    ~EmbeddedWindow() override;

    void attach(xcb_window_t xid);
    xcb_window_t xid() const { return xid_; }
    QString title() const { return title_; }

    void setActive(bool on);
    bool isActive() const { return active_; }
    void setTitleIcon(const QIcon &icon);
    void setWindowTitleText(const QString &title);
    void minimize();
    void restore();
    bool isMinimized() const { return minimized_; }
    void toggleMaximize();
    bool isMaximized() const { return maximized_; }

    static constexpr int TITLEBAR_H = 32;

signals:
    void closeRequested(EmbeddedWindow *w);
    void stateChanged(EmbeddedWindow *w);
    void activated(EmbeddedWindow *w);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void moveEvent(QMoveEvent *event) override;

private:
    void syncChildGeometry();
    void applyStyle();
    void dragFromTitlebar(QMouseEvent *event);

    QWidget *titlebar_ = nullptr;
    QWidget *content_ = nullptr;
    QLabel *iconLabel_ = nullptr;
    QLabel *titleLabel_ = nullptr;
    QPushButton *minBtn_ = nullptr;
    QPushButton *maxBtn_ = nullptr;
    QPushButton *closeBtn_ = nullptr;
    xcb_window_t xid_ = 0;
    QString title_;
    bool active_ = false;
    bool minimized_ = false;
    bool maximized_ = false;
    QRect restoreGeom_;
    bool dragging_ = false;
    QPoint dragOffset_;
};
