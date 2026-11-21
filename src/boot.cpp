#include "boot.h"

#include <QPainter>
#include <QStandardPaths>
#include <QRandomGenerator>

#include "theme.h"

namespace {
constexpr int PHASE1_MS = 2000; // streaming code output
constexpr int PHASE2_MS = 3000; // macOS-style progress bar
constexpr int FADE_MS = 320;

const QStringList LOG_POOL = {
    QStringLiteral("nvazen kernel: Booting NVazen Linux 26.11 (x86_64)"),
    QStringLiteral("nvazen kernel: ACPI: initialised PM bus"),
    QStringLiteral("systemd[1]: Starting Azen Sandbox…"),
    QStringLiteral("azen-sandbox: mounting toy filesystem… done"),
    QStringLiteral("azen-sandbox: loading window manager… ok"),
    QStringLiteral("nvazen kernel: USB: 4 devices found"),
    QStringLiteral("systemd[1]: Reached target Toy Desktop."),
    QStringLiteral("azen-sandbox: adopting X display :1"),
    QStringLiteral("azen-sandbox: compositing: redirect enabled"),
    QStringLiteral("azen-sandbox: compiling whale emoji … done"),
    QStringLiteral("wine[1]: initialising Windows subsystem…"),
    QStringLiteral("wine[1]: C:\\windows ready"),
    QStringLiteral("systemd[1]: Started Network Manager."),
    QStringLiteral("systemd[1]: Started Sound Service."),
    QStringLiteral("nvazen kernel: GPU: rendering cute bubbles"),
    QStringLiteral("azen-sandbox: wallpaper loaded (blue bubbles)"),
    QStringLiteral("azen-sandbox: taskbar online — height 48px"),
    QStringLiteral("azen-sandbox: start menu prepared (Wine: pending)"),
    QStringLiteral("systemd[1]: Reached target Multi-Toy System."),
    QStringLiteral("nvazen kernel: entropy: whale noises detected"),
    QStringLiteral("azen-sandbox: fonts OK (Noto Sans CJK SC)"),
    QStringLiteral("azen-sandbox: theme loaded from kdeglobals"),
    QStringLiteral("nvazen kernel: ACPI: lid closed (it's a window)"),
    QStringLiteral("systemd[1]: Finished File System Check."),
    QStringLiteral("azen-sandbox: opening tiny doors… 3, 2, 1"),
};
} // namespace

BootSplash::BootSplash(QWidget *parent) : QWidget(parent) {
    setObjectName("bootSplash");
    setAttribute(Qt::WA_OpaquePaintEvent);

    const QString logoPath =
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation) +
        QStringLiteral("/assets/NVazen Linux.png");
    logo_ = QPixmap(logoPath);

    logPool_ = LOG_POOL;

    timer_ = new QTimer(this);
    timer_->setInterval(30);
    connect(timer_, &QTimer::timeout, this, &BootSplash::tick);
    clock_.start();
    timer_->start();
}

QString BootSplash::randomHexDump() {
    QString hex;
    for (int i = 0; i < 10; ++i)
        hex += QStringLiteral("%1 ").arg(QRandomGenerator::global()->bounded(0x10000), 4, 16,
                                         QLatin1Char('0'));
    return QStringLiteral("  mem: ") + hex.trimmed();
}

void BootSplash::addLogLines(int count) {
    for (int i = 0; i < count; ++i) {
        QString line;
        if (QRandomGenerator::global()->bounded(9) == 0)
            line = randomHexDump();
        else
            line = logPool_.at(QRandomGenerator::global()->bounded(logPool_.size()));
        logLines_.append(line);
    }
    while (logLines_.size() > 40)
        logLines_.removeFirst();
}

void BootSplash::tick() {
    const qint64 t = clock_.elapsed();

    if (t < PHASE1_MS) {
        // Jerky, bursty output — like a real boot log, not a smooth animation.
        const int r = QRandomGenerator::global()->bounded(100);
        if (r < 45)
            addLogLines(1 + QRandomGenerator::global()->bounded(2));
        else if (r < 52)
            addLogLines(4 + QRandomGenerator::global()->bounded(6)); // sudden burst
        // otherwise: stall
    } else if (t < PHASE1_MS + PHASE2_MS) {
        // Stuttery progress: random steps with stalls, like a real loader.
        const int r = QRandomGenerator::global()->bounded(100);
        if (r < 55)
            progress_ += 0.008 + QRandomGenerator::global()->bounded(24) / 1000.0;
        if (t >= PHASE1_MS + PHASE2_MS - 80)
            progress_ = 1.0;
    } else if (!finishing_) {
        finishing_ = true;
    } else {
        fade_ -= 30.0 / FADE_MS;
        if (fade_ <= 0.0) {
            timer_->stop();
            emit finished();
            return;
        }
    }
    update();
}

void BootSplash::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.setOpacity(fade_);
    p.fillRect(rect(), QColor(7, 7, 10));

    const qint64 t = clock_.elapsed();

    // ---- the code output floods most of the window ----
    const double logAlpha = finishing_ ? fade_ : (t < PHASE1_MS ? 1.0 : 1.0 - (t - PHASE1_MS) / 1300.0);
    if (logAlpha > 0.0 && !logLines_.isEmpty()) {
        p.setOpacity(fade_ * qMax(0.0, logAlpha));
        QFont mono(QStringLiteral("Monospace"), 11);
        mono.setStyleHint(QFont::Monospace);
        p.setFont(mono);
        const int lineH = 18;
        const int maxLines = qMax(4, (height() - 60) / lineH);
        QStringList visible = logLines_;
        while (visible.size() > maxLines)
            visible.removeFirst();
        const int startY = 24;
        for (int i = 0; i < visible.size(); ++i) {
            const int hash = qHash(visible.at(i)) % 10;
            QColor color(232, 232, 232); // mostly white
            if (hash == 8)
                color = QColor(88, 200, 88);  // occasional green
            else if (hash == 9)
                color = QColor(224, 85, 85);  // occasional red
            color.setAlpha(215);
            p.setPen(color);
            const QString prefix = QStringLiteral("[ %1.%2 ] ").arg(
                (t / 1000) % 60, 2, 10, QLatin1Char('0')).arg((t / 10) % 100, 2, 10, QLatin1Char('0'));
            p.drawText(QRect(26, startY + i * lineH, width() - 52, lineH),
                       Qt::AlignLeft, prefix + visible.at(i));
        }
    }

    // ---- logo, centre stage, on a soft dark plate for readability ----
    const int logoSize = int(150 + 60 * qMin(1.0, t / 600.0));
    QRect logoRect((width() - logoSize) / 2, int(height() * 0.36) - logoSize / 2,
                   logoSize, logoSize);
    double logoAlpha = qMin(1.0, t / 500.0);
    if (finishing_)
        logoAlpha = fade_;
    p.setOpacity(fade_ * logoAlpha);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(7, 7, 10, 165));
    p.drawRoundedRect(logoRect.adjusted(-34, -26, 34, 26), 24, 24);
    if (!logo_.isNull()) {
        p.drawPixmap(logoRect, logo_.scaled(logoRect.size(), Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation));
    } else {
        p.setPen(Qt::white);
        QFont f = gTheme.font(6);
        f.setBold(true);
        p.setFont(f);
        p.drawText(logoRect, Qt::AlignCenter, QStringLiteral("NVazen Linux"));
    }
    p.setOpacity(fade_);

    // ---- progress bar (phase 2) ----
    if (t >= PHASE1_MS) {
        const int barW = 300, barH = 6;
        const int barX = (width() - barW) / 2;
        const int barY = int(height() * 0.36) + logoSize / 2 + 52;

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 38));
        p.drawRoundedRect(barX, barY, barW, barH, 3, 3);

        const int fillW = int(barW * qBound(0.0, progress_, 1.0));
        if (fillW > 0) {
            QLinearGradient g(barX, 0, barX + barW, 0);
            g.setColorAt(0, gTheme.accentHover);
            g.setColorAt(1, gTheme.accent);
            p.setBrush(g);
            p.drawRoundedRect(barX, barY, fillW, barH, 3, 3);
            p.setBrush(QColor(255, 255, 255, 110));
            p.drawRoundedRect(barX + 2, barY + 1, fillW - 4, 2, 1, 1);
        }

        p.setPen(QColor(255, 255, 255, 150));
        p.setFont(gTheme.font(-2));
        p.drawText(QRect(barX, barY + 16, barW, 20), Qt::AlignCenter,
                   QStringLiteral("Starting NVazen…"));
    }
}
