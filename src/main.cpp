#include <QApplication>
#include <QIcon>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>

#include "sandbox.h"
#include "theme.h"
#include "x11.h"

int main(int argc, char *argv[]) {
    // The sandbox hosts foreign X windows, so it always needs the xcb
    // backend (on a Wayland session this lands on XWayland).
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "xcb");

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Azen Sandbox"));
    app.setOrganizationName(QStringLiteral("AzenDev"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));

    gTheme = Theme::load();
    app.setFont(gTheme.font());

    // Draw a little whale as the window icon.
    QPixmap pm(64, 64);
    pm.fill(Qt::transparent);
    {
        QPainter p(&pm);
        p.setFont(QFont(QStringLiteral("Noto Color Emoji"), 40));
        p.drawText(pm.rect(), Qt::AlignCenter, QStringLiteral("🐳"));
    }
    app.setWindowIcon(QIcon(pm));

    if (!X11::connection()) {
        QMessageBox::critical(
            nullptr, QStringLiteral("Azen Sandbox"),
            QStringLiteral(
                "Azen Sandbox needs an X11 / XWayland display to host app windows,\n"
                "but this session does not provide one."));
        return 1;
    }

    SandboxWindow window;
    window.show();
    return app.exec();
}
