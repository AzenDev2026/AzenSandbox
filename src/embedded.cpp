#include "embedded.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

#include "theme.h"
#include "x11.h"

EmbeddedWindow::EmbeddedWindow(const QString &title, QWidget *parent)
    : QWidget(parent), title_(title) {
    setObjectName("embedFrame");
    setAttribute(Qt::WA_DeleteOnClose);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    titlebar_ = new QWidget(this);
    titlebar_->setObjectName("embedTitlebar");
    titlebar_->setFixedHeight(TITLEBAR_H);
    titlebar_->setMouseTracking(true);

    auto *bar = new QHBoxLayout(titlebar_);
    bar->setContentsMargins(8, 0, 4, 0);
    bar->setSpacing(4);

    iconLabel_ = new QLabel(titlebar_);
    iconLabel_->setFixedSize(18, 18);
    iconLabel_->setAlignment(Qt::AlignCenter);
    iconLabel_->setText(QStringLiteral("🐳"));
    bar->addWidget(iconLabel_);

    titleLabel_ = new QLabel(title, titlebar_);
    titleLabel_->setObjectName("embedTitle");
    titleLabel_->setStyleSheet(QStringLiteral("border: none; background: transparent;"));
    bar->addWidget(titleLabel_, 1);

    auto makeButton = [&](const QString &text, const QString &name) {
        auto *b = new QPushButton(text, titlebar_);
        b->setObjectName(name);
        b->setFixedSize(34, 26);
        b->setFocusPolicy(Qt::NoFocus);
        b->setCursor(Qt::ArrowCursor);
        bar->addWidget(b);
        return b;
    };
    minBtn_ = makeButton(QStringLiteral("–"), "embedMin");
    maxBtn_ = makeButton(QStringLiteral("□"), "embedMax");
    closeBtn_ = makeButton(QStringLiteral("✕"), "embedClose");

    content_ = new QWidget(this);
    content_->setObjectName("embedContent");
    content_->setAttribute(Qt::WA_NativeWindow);
    content_->setAttribute(Qt::WA_OpaquePaintEvent);
    content_->setAttribute(Qt::WA_PaintOnScreen);
    outer->addWidget(content_, 1);

    connect(minBtn_, &QPushButton::clicked, this, [this]() {
        minimized_ ? restore() : minimize();
    });
    connect(maxBtn_, &QPushButton::clicked, this, &EmbeddedWindow::toggleMaximize);
    connect(closeBtn_, &QPushButton::clicked, this, [this]() { emit closeRequested(this); });

    content_->installEventFilter(this);
    titlebar_->installEventFilter(this);
    applyStyle();
}

EmbeddedWindow::~EmbeddedWindow() = default;

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

void EmbeddedWindow::applyStyle() {
    setStyleSheet(QStringLiteral(
        "#embedFrame { background: %1; border: 1px solid %2; }"
        "#embedTitlebar { background: %3; }"
        "#embedTitle { color: %4; font-size: 12px; }"
        "#embedMin, #embedMax, #embedClose { border: none; background: transparent;"
        "  color: %4; font-size: 14px; border-radius: 4px; }"
        "#embedMin:hover, #embedMax:hover { background: %5; }"
        "#embedClose:hover { background: %6; color: white; }")
        .arg(active_ ? gTheme.viewBg.lighter(112).name() : gTheme.viewBg.name(),
             active_ ? gTheme.accent.name() : gTheme.border.name(),
             active_ ? gTheme.buttonBg.name() : gTheme.viewBg.name(),
             active_ ? gTheme.text.name() : gTheme.textDim.name(),
             gTheme.buttonBg.name(), gTheme.closeHover.name()));
}

void EmbeddedWindow::attach(xcb_window_t xid) {
    xid_ = xid;
    content_->winId(); // force the native window to exist
    X11::reparent(xid, static_cast<xcb_window_t>(content_->winId()));
    X11::moveResize(xid, 0, 0, content_->width(), content_->height());
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

void EmbeddedWindow::moveEvent(QMoveEvent *event) {
    QWidget::moveEvent(event);
    syncChildGeometry();
}

void EmbeddedWindow::syncChildGeometry() {
    if (!xid_)
        return;
    X11::moveResize(xid_, 0, 0, content_->width(), content_->height());
    X11::flush();
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
