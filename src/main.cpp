#include <QApplication>
#include <QIcon>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QStandardPaths>
#include <QTimer>

#include <csignal>
#include <cstdlib>
#include <unistd.h>
#include <xcb/xcb.h>

#include "sandbox.h"
#include "theme.h"
#include "x11.h"

// The sandbox runs inside its own nested X server (Xephyr). Reparenting app
// windows is 100% legal on a real X server, but on XWayland it can wedge
// clients in an unkillable state, so the nested display is the primary mode.

static pid_t g_xephyrPid = -1;

static void killXephyr() {
    if (g_xephyrPid > 0) {
        ::kill(g_xephyrPid, SIGTERM);
        g_xephyrPid = -1;
    }
}

static bool displayReady(const char *name) {
    for (int i = 0; i < 60; ++i) {
        xcb_connection_t *c = xcb_connect(name, nullptr);
        if (c) {
            const bool ok = xcb_connection_has_error(c) == 0;
            xcb_disconnect(c);
            if (ok)
                return true;
        }
        usleep(100000); // 100 ms
    }
    return false;
}

//: Try to launch the nested X server. Returns true when DISPLAY now points
//   at the nested display.
static bool enterNestedDisplay() {
    if (qEnvironmentVariableIsSet("AZS_INSIDE_XEPHYR"))
        return true; // already inside (this run was re-spawned on :101)
    if (qEnvironmentVariableIsSet("AZS_TEST") || qEnvironmentVariableIsSet("AZS_NO_XEPHYR"))
        return false;

    const QString xephyr = QStandardPaths::findExecutable(QStringLiteral("Xephyr"));
    if (xephyr.isEmpty())
        return false;

    const pid_t pid = fork();
    if (pid == 0) { // child: become the nested X server
        execl(xephyr.toLocal8Bit().constData(), "Xephyr", ":101",
              "-screen", "1280x720",
              "-title", "Azen Sandbox",
              "-br", "-ac", "-reset", "-resizeable",
              static_cast<char *>(nullptr));
        _exit(127);
    }
    if (pid < 0)
        return false;

    g_xephyrPid = pid;
    if (!displayReady(":101")) {
        // Xephyr failed to come up; fall back to the real display.
        fprintf(stderr, "XEPHYR_FALLBACK: nested display did not come up\n");
        ::kill(pid, SIGTERM);
        g_xephyrPid = -1;
        return false;
    }

    fprintf(stderr, "XEPHYR_OK: nested display :101 is up\n");
    qputenv("DISPLAY", ":101");
    qputenv("QT_QPA_PLATFORM", "xcb");
    qputenv("AZS_INSIDE_XEPHYR", "1");
    atexit(killXephyr);
    return true;
}

int main(int argc, char *argv[]) {
    // Nested mode comes first: on the real desktop (Wayland/XWayland) the
    // sandbox starts its own Xephyr display and re-runs itself inside it.
    const bool nested = enterNestedDisplay();

    if (!nested && !qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
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
    if (nested) {
        // Inside the nested display there is no window manager: run
        // frameless and size the window to the screen EXACTLY (a mismatch
        // clips the taskbar below the screen edge). The sandbox follows
        // nested-screen resizes via root ConfigureNotify.
        window.setWindowFlags(window.windowFlags() | Qt::FramelessWindowHint);
        const QSize screenSize = X11::screenSize();
        window.setFixedSize(screenSize.width() > 0 ? screenSize : QSize(1280, 720));
        window.move(0, 0);
        window.show();
    } else {
        if (!qEnvironmentVariableIsSet("AZS_TEST")) {
            // After the boot splash (5 s) let the user know about fallback
            // mode, so the notice doesn't cover the animation.
            QTimer::singleShot(6000, &window, [&window]() {
                window.showToast(QStringLiteral(
                    "⚠ Xephyr not found — fallback mode on the real display. "
                    "Install it with: sudo pacman -S xorg-server-xephyr"));
            });
        }
        window.show();
    }
    return app.exec();
}
