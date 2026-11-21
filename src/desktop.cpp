#include "desktop.h"

#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QToolButton>
#include <QVBoxLayout>

#include "apps.h"
#include "theme.h"

DesktopWidget::DesktopWidget(QWidget *parent) : QWidget(parent) {
    if (!gTheme.wallpaperPath.isEmpty() && QFileInfo(gTheme.wallpaperPath).isFile())
        wallpaper_ = QPixmap(gTheme.wallpaperPath);
    buildIcons();
}

void DesktopWidget::buildIcons() {
    // A handful of pinned favourites, resolved against installed apps.
    const QStringList pins = {QStringLiteral("org.kde.konsole"),
                              QStringLiteral("org.kde.dolphin"),
                              QStringLiteral("firefox"),
                              QStringLiteral("org.kde.gwenview"),
                              QStringLiteral("org.kde.ark")};
    const QVector<DesktopApp> apps = AppList::load();

    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(18, 18, 0, 0);
    column->setSpacing(4);

    auto iconButton = [this](const DesktopApp &app) {
        auto *b = new QToolButton(this);
        b->setObjectName("desktopIcon");
        b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        b->setIcon(app.themedIcon());
        b->setIconSize(QSize(40, 40));
        b->setText(app.name);
        b->setFixedSize(88, 78);
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QToolButton::clicked, this, [this, id = app.id]() {
            emit launchRequested(id);
        });
        return b;
    };

    for (const QString &pin : pins) {
        const DesktopApp *found = nullptr;
        for (const DesktopApp &a : apps) {
            if (a.id == pin || a.id == pin + QStringLiteral(".desktop")) {
                found = &a;
                break;
            }
        }
        if (found)
            column->addWidget(iconButton(*found));
    }
    column->addStretch(1);
    column->addWidget(new QLabel(QStringLiteral("Azen Sandbox 🐳"), this));
    setStyleSheet(QStringLiteral(
        "#desktopIcon { color: white; background: rgba(0,0,0,0.35); border: none;"
        "  border-radius: 8px; font-size: 11px; }"
        "#desktopIcon:hover { background: rgba(0,0,0,0.55); }"
        "QLabel { color: rgba(255,255,255,0.6); font-size: 10px; }"));
}

void DesktopWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter p(this);
    if (wallpaper_.isNull()) {
        // Fallback: a soft dark gradient with a hint of the accent colour.
        QLinearGradient g(0, 0, width(), height());
        g.setColorAt(0, gTheme.viewBg);
        g.setColorAt(1, QColor(gTheme.accent).darker(220));
        p.fillRect(rect(), g);
    } else {
        p.fillRect(rect(), Qt::black);
        const QPixmap scaled = wallpaper_.scaled(size(), Qt::KeepAspectRatioByExpanding,
                                                 Qt::SmoothTransformation);
        const int x = (width() - scaled.width()) / 2;
        const int y = (height() - scaled.height()) / 2;
        p.drawPixmap(x, y, scaled);
    }
    // Subtle bottom-right watermark.
    p.setPen(QColor(255, 255, 255, 70));
    p.setFont(gTheme.font(-1));
    p.drawText(rect().adjusted(0, 0, -16, -14), Qt::AlignRight | Qt::AlignBottom,
               QStringLiteral("Azen Sandbox — a tiny world for your apps"));
}
