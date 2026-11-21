// Tiny xcb helpers. All calls go through Qt's own X connection so that the
// events we select on foreign windows arrive through the native event filter.
#pragma once

#include <QImage>
#include <QString>
#include <QVector>

#include <xcb/xcb.h>
#include <xcb/composite.h>
#include <xcb/damage.h>
#include <xcb/shape.h>

class X11 {
public:
    //: Qt's xcb connection (nullptr when the platform is not xcb).
    static xcb_connection_t *connection();
    static xcb_window_t rootWindow();
    static QSize screenSize();
    static void flush();

    static QVector<xcb_window_t> topLevelWindows();
    static bool windowPid(xcb_window_t w, long *pidOut);
    static bool hasWmClass(xcb_window_t w); // legacy apps lack _NET_WM_PID
    static long processGroupOf(long pid); // from /proc/<pid>/stat
    static QString windowTitle(xcb_window_t w);
    static bool windowAlive(xcb_window_t w);

    static void reparent(xcb_window_t child, xcb_window_t parent);
    static void moveResize(xcb_window_t w, int x, int y, int width, int height);
    //: Round the BOTTOM corners of the window with XShape (no-op when the
    //   extension is missing) so the app content matches the rounded frame.
    static void roundBottomCorners(xcb_window_t w, int width, int height, int radius);
    static void mapWindow(xcb_window_t w);
    static void unmapWindow(xcb_window_t w);
    static void raiseWindow(xcb_window_t w);
    static void setInputFocus(xcb_window_t w);
    static void selectEmbedEvents(xcb_window_t w);
    static void requestClose(xcb_window_t w); // WM_DELETE_WINDOW ClientMessage
    static void killClientConnection(xcb_window_t w);

    // ---- compositing (the sandbox draws app windows itself) ----
    static bool compositeAvailable();
    //: Redirect the window off-screen so its pixels stop showing anywhere;
    //   the sandbox then repaints them itself.
    static bool redirectWindow(xcb_window_t w);
    //: Grab the redirected window's current contents as an image.
    static QImage windowContents(xcb_window_t w);
    static xcb_damage_damage_t createDamage(xcb_window_t w);
    static void subtractDamage(xcb_damage_damage_t damage);
    static uint8_t damageNotifyEvent();
    //: Subscribe to map events on the root so freshly created windows can be
    //   adopted (and redirected) the moment they appear.
    static void watchRootSubstructure();

private:
    static xcb_atom_t atom(const char *name);
};
