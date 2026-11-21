// A fully self-drawn Windows-11-style window frame. The real app window is
// reparented into the content area and renders directly, with XShape-rounded
// bottom corners; no original decoration of any app is ever shown.
#pragma once

#include <QWidget>
#include <xcb/xcb.h>

class QLabel;

class CaptionButton : public QWidget {
    Q_OBJECT
public:
    enum Kind { Minimize, Maximize, Close };
    explicit CaptionButton(Kind kind, QWidget *parent = nullptr);

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    Kind kind_;
    bool hover_ = false;
    bool pressed_ = false;
};

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

    //: Follow the app's own initial size negotiation ONCE (dialog-sized
    //   windows shrink to fit); later self-resizes are ignored.
    void adoptAppSize(const QSize &size);

    //: Ask the sandbox to close this window (same as clicking ✕).
    void requestClose();

    static constexpr int TITLEBAR_H = 36;
    static constexpr int SHADOW = 16;   // margin reserved for the drop shadow
    static constexpr int RADIUS = 8;

signals:
    void closeRequested(EmbeddedWindow *w);
    void stateChanged(EmbeddedWindow *w);
    void activated(EmbeddedWindow *w);

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void syncChildGeometry();
    void applyStyle();
    void dragFromTitlebar(QMouseEvent *event);
    QWidget *frame_ = nullptr;   // rounded surface inside the shadow margin
    QWidget *titlebar_ = nullptr;
    QWidget *content_ = nullptr;
    QLabel *iconLabel_ = nullptr;
    QLabel *titleLabel_ = nullptr;
    CaptionButton *minBtn_ = nullptr;
    CaptionButton *maxBtn_ = nullptr;
    CaptionButton *closeBtn_ = nullptr;
    xcb_window_t xid_ = 0;
    QString title_;
    bool active_ = false;
    bool minimized_ = false;
    bool maximized_ = false;
    bool sizeAdopted_ = false;
    QRect restoreGeom_;
    bool dragging_ = false;
    QPoint dragOffset_;
};
