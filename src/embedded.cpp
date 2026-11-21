#include "embedded.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QVBoxLayout>

#include "theme.h"
#include "x11.h"

// ---------------------------------------------------------------- caption --

CaptionButton::CaptionButton(Kind kind, QWidget *parent) : QWidget(parent), kind_(kind) {
    setFixedSize(46, 36);
    setCursor(Qt::ArrowCursor);
    setMouseTracking(true);
}

void CaptionButton::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QColor bg = Qt::transparent;
    if (hover_) {
        bg = (kind_ == Close) ? QColor(196, 43, 28) : QColor(255, 255, 255, 26);
    } else if (pressed_) {
        bg = (kind_ == Close) ? QColor(170, 35, 24) : QColor(255, 255, 255, 40);
    }
    if (bg.alpha() > 0) {
        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        p.drawRoundedRect(rect().adjusted(7, 4, -7, -4), 4, 4);
    }

    p.setPen(QPen(QColor(230, 230, 230), 1.2));
    const QRectF area = rect().adjusted(16, 10, -16, -10);
    switch (kind_) {
    case Minimize:
        p.drawLine(QPointF(area.left(), area.center().y()),
                   QPointF(area.right(), area.center().y()));
        break;
    case Maximize:
        p.drawRoundedRect(area, 1.5, 1.5);
        break;
    case Close:
        p.drawLine(area.topLeft(), area.bottomRight());
        p.drawLine(area.topRight(), area.bottomLeft());
        break;
    }
}

void CaptionButton::enterEvent(QEnterEvent *) { hover_ = true; update(); }
void CaptionButton::leaveEvent(QEvent *) { hover_ = false; update(); }
void CaptionButton::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton) {
        pressed_ = true;
        update();
    }
}
void CaptionButton::mouseReleaseEvent(QMouseEvent *e) {
    const bool wasPressed = pressed_;
    pressed_ = false;
    update();
    if (e->button() == Qt::LeftButton && wasPressed && rect().contains(e->position().toPoint()))
        emit clicked();
}

// ------------------------------------------------------------- main frame --

EmbeddedWindow::EmbeddedWindow(const QString &title, QWidget *parent)
    : QWidget(parent), title_(title) {
    setObjectName("embedFrame");
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_TranslucentBackground);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(SHADOW, SHADOW, SHADOW, SHADOW);
    outer->setSpacing(0);

    frame_ = new QWidget(this);
    frame_->setObjectName("embedSurface");
    auto *frameLayout = new QVBoxLayout(frame_);
    frameLayout->setContentsMargins(1, 1, 1, 1);
    frameLayout->setSpacing(0);

    titlebar_ = new QWidget(frame_);
    titlebar_->setFixedHeight(TITLEBAR_H);
    titlebar_->setMouseTracking(true);

    auto *bar = new QHBoxLayout(titlebar_);
    bar->setContentsMargins(10, 0, 2, 0);
    bar->setSpacing(0);

    iconLabel_ = new QLabel(titlebar_);
    iconLabel_->setFixedSize(20, 20);
    iconLabel_->setAlignment(Qt::AlignCenter);
    bar->addWidget(iconLabel_);
    bar->addSpacing(8);

    // Left spacer balances the caption buttons so the title is centred.
    bar->addStretch(1);
    titleLabel_ = new QLabel(title, titlebar_);
    titleLabel_->setAlignment(Qt::AlignCenter);
    bar->addWidget(titleLabel_, 0);
    bar->addStretch(1);
    // Reclaim the 138 px the buttons need so the title truly centres.
    auto *balance = new QWidget(titlebar_);
    balance->setFixedWidth(46 * 3 - 10 - 8);
    bar->addWidget(balance);

    minBtn_ = new CaptionButton(CaptionButton::Minimize, titlebar_);
    maxBtn_ = new CaptionButton(CaptionButton::Maximize, titlebar_);
    closeBtn_ = new CaptionButton(CaptionButton::Close, titlebar_);
    bar->addWidget(minBtn_);
    bar->addWidget(maxBtn_);
    bar->addWidget(closeBtn_);

    frameLayout->addWidget(titlebar_);

    content_ = new QWidget(frame_);
    content_->setObjectName("embedContent");
    content_->setAttribute(Qt::WA_NativeWindow);
    content_->setAttribute(Qt::WA_OpaquePaintEvent);
    content_->setAttribute(Qt::WA_PaintOnScreen);
    frameLayout->addWidget(content_, 1);

    outer->addWidget(frame_);

    connect(minBtn_, &CaptionButton::clicked, this, [this]() {
        minimized_ ? restore() : minimize();
    });
    connect(maxBtn_, &CaptionButton::clicked, this, &EmbeddedWindow::toggleMaximize);
    connect(closeBtn_, &CaptionButton::clicked, this, [this]() { emit closeRequested(this); });

    content_->installEventFilter(this);
    titlebar_->installEventFilter(this);
    applyStyle();
}

EmbeddedWindow::~EmbeddedWindow() = default;

void EmbeddedWindow::applyStyle() {
    titleLabel_->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 12px; background: transparent;")
        .arg(active_ ? gTheme.text.name() : gTheme.textDim.name()));
    update();
}

void EmbeddedWindow::setTitleIcon(const QIcon &icon) {
    if (icon.isNull())
        return;
    iconLabel_->setPixmap(icon.pixmap(16, 16));
}

void EmbeddedWindow::setWindowTitleText(const QString &title) {
    if (title == title_)
        return;
    title_ = title;
    titleLabel_->setText(title);
}

void EmbeddedWindow::attach(xcb_window_t xid) {
    xid_ = xid;
    content_->winId(); // force the native window to exist
    X11::reparent(xid, static_cast<xcb_window_t>(content_->winId()));
    X11::moveResize(xid, 0, 0, content_->width(), content_->height());
    X11::roundBottomCorners(xid, content_->width(), content_->height(), RADIUS);
    X11::mapWindow(xid);
    X11::selectEmbedEvents(xid);
    X11::flush();
}

void EmbeddedWindow::setActive(bool on) {
    if (active_ == on)
        return;
    active_ = on;
    applyStyle();
    if (on)
        emit activated(this);
}

void EmbeddedWindow::minimize() {
    if (minimized_)
        return;
    minimized_ = true;
    X11::unmapWindow(xid_);
    X11::flush();
    hide();
    emit stateChanged(this);
}

void EmbeddedWindow::restore() {
    if (!minimized_)
        return;
    minimized_ = false;
    show();
    X11::mapWindow(xid_);
    X11::raiseWindow(xid_);
    X11::flush();
    emit stateChanged(this);
}

void EmbeddedWindow::toggleMaximize() {
    if (maximized_) {
        maximized_ = false;
        setGeometry(restoreGeom_);
    } else {
        maximized_ = true;
        restoreGeom_ = geometry();
        QWidget *desk = parentWidget();
        const QRect area = desk ? desk->rect().adjusted(8, 8, -8, -8) : geometry();
        setGeometry(area);
    }
    syncChildGeometry();
    emit stateChanged(this);
}

bool EmbeddedWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched == content_ && xid_) {
        if (event->type() == QEvent::Resize)
            syncChildGeometry();
    } else if (watched == titlebar_) {
        if (event->type() == QEvent::MouseButtonPress ||
            event->type() == QEvent::MouseMove ||
            event->type() == QEvent::MouseButtonRelease) {
            dragFromTitlebar(static_cast<QMouseEvent *>(event));
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void EmbeddedWindow::syncChildGeometry() {
    if (!xid_)
        return;
    const int w = content_->width();
    const int h = content_->height();
    X11::moveResize(xid_, 0, 0, w, h);
    X11::roundBottomCorners(xid_, w, h, RADIUS);
    X11::flush();
}

void EmbeddedWindow::adoptAppSize(const QSize &size) {
    if (sizeAdopted_ || maximized_ || minimized_)
        return;
    sizeAdopted_ = true;
    const int w = qBound(240, size.width(), 2000);
    const int h = qBound(150, size.height(), 2000);
    resize(w, h + TITLEBAR_H); // content follows via the layout
}

void EmbeddedWindow::requestClose() {
    emit closeRequested(this);
}

void EmbeddedWindow::dragFromTitlebar(QMouseEvent *event) {
    if (event->type() == QEvent::MouseButtonPress && event->button() == Qt::LeftButton) {
        dragging_ = true;
        dragOffset_ = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    } else if (event->type() == QEvent::MouseMove && dragging_) {
        move(event->globalPosition().toPoint() - dragOffset_);
        event->accept();
    } else if (event->type() == QEvent::MouseButtonRelease && dragging_) {
        dragging_ = false;
        event->accept();
    }
}

void EmbeddedWindow::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF surface = QRectF(rect()).adjusted(SHADOW, SHADOW, -SHADOW, -SHADOW);

    // Soft drop shadow.
    const int layers = active_ ? 4 : 3;
    for (int i = 0; i < layers; ++i) {
        const double grow = (i + 1) * 2.5;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, int((active_ ? 26 : 18) * (layers - i) / layers)));
        p.drawRoundedRect(surface.adjusted(-grow, -grow + i, grow, grow + i),
                          RADIUS + grow, RADIUS + grow);
    }

    // Window surface.
    p.setPen(QPen(active_ ? QColor(gTheme.accent).darker(140) : QColor(45, 45, 50), 1));
    p.setBrush(active_ ? QColor(35, 37, 42) : QColor(30, 30, 33));
    p.drawRoundedRect(surface, RADIUS, RADIUS);
}
