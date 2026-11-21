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
    QStringLiteral("azen-sandbox: xcb shape extension present"),
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
    for (int i = 0; i < 8; ++i)
        hex += QStringLiteral("%1 ").arg(QRandomGenerator::global()->bounded(0x10000), 4, 16,
                                         QLatin1Char('0'));
    return QStringLiteral("  mem: ") + hex.trimmed();
}

void BootSplash::addLogLines(int count) {
    for (int i = 0; i < count; ++i) {
        QString line;
        if (QRandomGenerator::global()->bounded(8) == 0)
            line = randomHexDump();
        else
            line = logPool_.at(QRandomGenerator::global()->bounded(logPool_.size()));
        logLines_.append(line);
    }
    while (logLines_.size() > 16)
        logLines_.removeFirst();
}

void BootSplash::tick() {
    const qint64 t = clock_.elapsed();

    if (t < PHASE1_MS) {
        // Crazy code output: 1-3 lines per 30 ms tick.
        addLogLines(1 + QRandomGenerator::global()->bounded(3));
    } else if (t < PHASE1_MS + PHASE2_MS) {
        progress_ = double(t - PHASE1_MS) / PHASE2_MS;
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
    p.fillRect(rect(), QColor(9, 9, 12));

    const qint64 t = clock_.elapsed();

    // ---- logo, centre stage ----
    const int logoSize = int(150 + 60 * qMin(1.0, t / 600.0)); // scale in
    QRect logoRect((width() - logoSize) / 2, int(height() * 0.30) - logoSize / 2,
                   logoSize, logoSize);
    double logoAlpha = qMin(1.0, t / 500.0);
    if (finishing_)
        logoAlpha = fade_;
    p.setOpacity(fade_ * logoAlpha);
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
        const int barW = 280, barH = 6;
        const int barX = (width() - barW) / 2;
        const int barY = int(height() * 0.30) + logoSize / 2 + 46;

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 36));
        p.drawRoundedRect(barX, barY, barW, barH, 3, 3);

        const double eased = 1.0 - (1.0 - progress_) * (1.0 - progress_); // ease-out
        const int fillW = int(barW * qBound(0.0, eased, 1.0));
        if (fillW > 0) {
            QLinearGradient g(barX, 0, barX + barW, 0);
            g.setColorAt(0, gTheme.accentHover);
            g.setColorAt(1, gTheme.accent);
            p.setBrush(g);
            p.drawRoundedRect(barX, barY, fillW, barH, 3, 3);
            // shine
            p.setBrush(QColor(255, 255, 255, 110));
            p.drawRoundedRect(barX + 2, barY + 1, fillW - 4, 2, 1, 1);
        }

        p.setPen(QColor(255, 255, 255, 150));
        p.setFont(gTheme.font(-2));
        p.drawText(QRect(barX, barY + 16, barW, 20), Qt::AlignCenter,
                   QStringLiteral("Starting NVazen…"));
    }

    // ---- streaming code output (phase 1, fading out in phase 2) ----
    const double logAlpha = finishing_ ? fade_ : (t < PHASE1_MS ? 1.0 : 1.0 - (t - PHASE1_MS) / 700.0);
    if (logAlpha > 0.0 && !logLines_.isEmpty()) {
        p.setOpacity(fade_ * qMax(0.0, logAlpha));
        QFont mono = gTheme.font(-2);
        mono.setFamily(QStringLiteral("Monospace"));
        mono.setStyleHint(QFont::Monospace);
        p.setFont(mono);
        const int lineH = 17;
        const int startY = height() - 16 - lineH * logLines_.size();
        for (int i = 0; i < logLines_.size(); ++i) {
            const int y = startY + i * lineH;
            const int hash = qHash(logLines_.at(i)) % 5;
            QColor color;
            switch (hash) {
            case 0: color = QColor(126, 200, 126); break;  // green
            case 1: color = QColor(90, 200, 224); break;  // cyan
            case 2: color = QColor(220, 220, 220); break; // white
            case 3: color = QColor(200, 180, 120); break; // amber
            default: color = QColor(140, 140, 150); break; // dim
            }
            color.setAlpha(210);
            p.setPen(color);
            const QString prefix = QStringLiteral("[ %1 ] ").arg(
                (t / 100) % 60, 2, 10, QLatin1Char('0'));
            p.drawText(24, y, width() - 48, lineH, Qt::AlignLeft, prefix + logLines_.at(i));
        }
    }
}
