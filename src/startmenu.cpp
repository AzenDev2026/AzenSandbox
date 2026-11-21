#include "startmenu.h"

#include <QApplication>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardPaths>
#include <QToolButton>
#include <QVBoxLayout>

#include "apps.h"
#include "theme.h"

StartMenu::StartMenu(QWidget *parent) : QWidget(parent) {
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(620, 520);

    auto *effect = new QGraphicsDropShadowEffect(this);
    effect->setBlurRadius(28);
    effect->setOffset(0, 6);
    effect->setColor(QColor(0, 0, 0, 160));
    setGraphicsEffect(effect);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(14, 14, 14, 12);
    outer->setSpacing(10);

    search_ = new QLineEdit(this);
    search_->setPlaceholderText(QStringLiteral("Search apps…"));
    search_->setClearButtonEnabled(true);
    search_->setStyleSheet(QStringLiteral(
        "QLineEdit { background: %1; border: 1px solid %2; border-radius: 10px;"
        "  padding: 8px 12px; color: %3; font-size: 13px; }"
        "QLineEdit:focus { border-color: %4; }")
        .arg(gTheme.buttonBg.name(), gTheme.border.name(), gTheme.text.name(),
             gTheme.accent.name()));
    connect(search_, &QLineEdit::textChanged, this, &StartMenu::rebuild);
    outer->addWidget(search_);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollContent_ = new QWidget(scroll);
    grid_ = new QGridLayout(scrollContent_);
    grid_->setContentsMargins(0, 0, 0, 0);
    grid_->setSpacing(6);
    scroll->setWidget(scrollContent_);
    outer->addWidget(scroll, 1);

    // Wine row + shutdown.
    auto *bottom = new QHBoxLayout();
    bottom->setSpacing(8);
    wineLabel_ = new QLabel(this);
    wineLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;")
                                  .arg(gTheme.textDim.name()));
    bottom->addWidget(wineLabel_);
    bottom->addStretch(1);
    auto *shutdown = new QPushButton(QStringLiteral("⏻  Shut Down Sandbox"), this);
    shutdown->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: white; border: none; border-radius: 8px;"
        "  padding: 8px 14px; font-weight: bold; }"
        "QPushButton:hover { background: %2; }")
        .arg(gTheme.closeHover.name(), gTheme.closeHover.lighter(120).name()));
    connect(shutdown, &QPushButton::clicked, this, [this]() { emit shutdownRequested(); });
    bottom->addWidget(shutdown);
    outer->addLayout(bottom);

    rebuild(QString());
}

void StartMenu::popupAt(const QPoint &globalPos) {
    rebuild(search_->text());
    move(globalPos);
    show();
    search_->setFocus();
}

void StartMenu::setWineAvailable(bool available, const QString &version) {
    wineAvailable_ = available;
    wineVersion_ = version;
    rebuild(search_->text());
}

void StartMenu::rebuild(const QString &filter) {
    // Clear the grid.
    while (QLayoutItem *item = grid_->takeAt(0)) {
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }

    const QString needle = filter.trimmed();
    const QVector<DesktopApp> apps = AppList::load();

    auto appButton = [this](const DesktopApp &app) {
        auto *b = new QToolButton(scrollContent_);
        b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        b->setIcon(app.themedIcon());
        b->setIconSize(QSize(30, 30));
        b->setText(app.name);
        b->setFixedSize(140, 74);
        b->setToolTip(app.comment);
        b->setStyleSheet(QStringLiteral(
            "QToolButton { background: transparent; border: none; border-radius: 10px;"
            "  color: %1; font-size: 11px; }"
            "QToolButton:hover { background: rgba(255,255,255,0.08); }")
            .arg(gTheme.text.name()));
        connect(b, &QToolButton::clicked, this, [this, id = app.id]() {
            emit appLaunchRequested(id);
            hide();
        });
        return b;
    };

    int row = 0;
    // Wine section on top: it is the star of the show.
    auto wineButton = [this](const QString &emoji, const QString &name, const QString &exe) {
        auto *b = new QToolButton(scrollContent_);
        b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        b->setIcon(QIcon::fromTheme(QStringLiteral("wine")));
        b->setIconSize(QSize(30, 30));
        b->setText(emoji + QStringLiteral(" ") + name);
        b->setFixedSize(140, 74);
        b->setStyleSheet(QStringLiteral(
            "QToolButton { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.08);"
            "  border-radius: 10px; color: %1; font-size: 11px; }"
            "QToolButton:hover { background: rgba(255,255,255,0.10); }")
            .arg(gTheme.text.name()));
        connect(b, &QToolButton::clicked, this, [this, exe]() {
            emit wineAppRequested(exe);
            hide();
        });
        return b;
    };

    if (wineAvailable_) {
        grid_->addWidget(wineButton(QStringLiteral("📝"), QStringLiteral("Notepad"),
                                    QStringLiteral("notepad")), row, 0);
        grid_->addWidget(wineButton(QStringLiteral("🧮"), QStringLiteral("Calculator"),
                                    QStringLiteral("calc")), row, 1);
        grid_->addWidget(wineButton(QStringLiteral("⚙️"), QStringLiteral("Regedit"),
                                    QStringLiteral("regedit")), row, 2);
        auto *openExe = new QToolButton(scrollContent_);
        openExe->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        openExe->setIcon(QIcon::fromTheme(QStringLiteral("document-open")));
        openExe->setIconSize(QSize(30, 30));
        openExe->setText(QStringLiteral("Open a .exe…"));
        openExe->setFixedSize(140, 74);
        openExe->setStyleSheet(QStringLiteral(
            "QToolButton { background: transparent; border: 1px dashed rgba(255,255,255,0.25);"
            "  border-radius: 10px; color: %1; font-size: 11px; }"
            "QToolButton:hover { background: rgba(255,255,255,0.08); }")
            .arg(gTheme.text.name()));
        connect(openExe, &QToolButton::clicked, this, [this]() {
            emit wineExeFileRequested();
            hide();
        });
        grid_->addWidget(openExe, row, 3);
        ++row;
        wineLabel_->setText(QStringLiteral("🍷 Wine %1 — Windows apps run inside the sandbox")
                                .arg(wineVersion_));
    } else {
        wineLabel_->setText(QStringLiteral(
            "🍷 Wine not installed — Windows apps are disabled. Install with: sudo pacman -S wine"));
    }

    int col = 0;
    for (const DesktopApp &app : apps) {
        if (!needle.isEmpty() &&
            !app.name.contains(needle, Qt::CaseInsensitive) &&
            !app.id.contains(needle, Qt::CaseInsensitive) &&
            !app.comment.contains(needle, Qt::CaseInsensitive))
            continue;
        grid_->addWidget(appButton(app), row, col);
        if (++col >= 4) {
            col = 0;
            ++row;
        }
    }
    if (col != 0)
        ++row;
    grid_->setRowStretch(row, 1);
}

void StartMenu::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(gTheme.border, 1));
    p.setBrush(gTheme.windowBg);
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 14, 14);
}
