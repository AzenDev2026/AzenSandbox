#include "x11.h"

#include <QFile>
#include <QGuiApplication>
#include <QHash>
#include <QStringList>
#include <QtGui/qpa/qplatformnativeinterface.h>

#include <cmath>

namespace {

xcb_atom_t intern(const char *name) {
    static QHash<QString, xcb_atom_t> cache;
    xcb_connection_t *c = X11::connection();
    if (!c)
        return XCB_ATOM_NONE;
    const QString key = QString::fromLatin1(name);
    if (cache.contains(key))
        return cache.value(key);
    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(c, 0, static_cast<uint16_t>(key.size()), name);
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(c, cookie, nullptr);
    xcb_atom_t atom = XCB_ATOM_NONE;
    if (reply) {
        atom = reply->atom;
        free(reply);
    }
    cache.insert(key, atom);
    return atom;
}

QString readTextProperty(xcb_window_t w, xcb_atom_t property, xcb_atom_t type) {
    xcb_connection_t *c = X11::connection();
    if (!c || property == XCB_ATOM_NONE)
        return QString();
    xcb_get_property_cookie_t cookie = xcb_get_property(c, 0, w, property, type, 0, 1024);
    xcb_get_property_reply_t *reply = xcb_get_property_reply(c, cookie, nullptr);
    QString out;
    if (reply && reply->value_len > 0) {
        const char *data = static_cast<const char *>(xcb_get_property_value(reply));
        out = QString::fromUtf8(data, reply->value_len);
    }
    if (reply)
        free(reply);
    return out;
}

} // namespace

xcb_connection_t *X11::connection() {
    static xcb_connection_t *conn = nullptr;
    static bool tried = false;
    if (!tried) {
        tried = true;
        QGuiApplication *app = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
        if (app && app->platformName() == QLatin1String("xcb")) {
            void *resource = app->platformNativeInterface()->nativeResourceForIntegration("connection");
            conn = static_cast<xcb_connection_t *>(resource);
        }
    }
    return conn;
}

xcb_window_t X11::rootWindow() {
    xcb_connection_t *c = connection();
    if (!c)
        return 0;
    xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
    return screen ? screen->root : 0;
}

void X11::flush() {
    if (xcb_connection_t *c = connection())
        xcb_flush(c);
}

QVector<xcb_window_t> X11::topLevelWindows() {
    QVector<xcb_window_t> out;
    xcb_connection_t *c = connection();
    const xcb_window_t root = rootWindow();
    if (!c || !root)
        return out;
    xcb_query_tree_cookie_t cookie = xcb_query_tree(c, root);
    xcb_query_tree_reply_t *reply = xcb_query_tree_reply(c, cookie, nullptr);
    if (!reply)
        return out;
    xcb_window_t *children = xcb_query_tree_children(reply);
    const int n = xcb_query_tree_children_length(reply);
    for (int i = 0; i < n; ++i)
        out.append(children[i]);
    free(reply);
    return out;
}

bool X11::windowPid(xcb_window_t w, long *pidOut) {
    const xcb_atom_t netWmPid = intern("_NET_WM_PID");
    xcb_connection_t *c = connection();
    if (!c || netWmPid == XCB_ATOM_NONE)
        return false;
    xcb_get_property_cookie_t cookie =
        xcb_get_property(c, 0, w, netWmPid, XCB_ATOM_CARDINAL, 0, 1);
    xcb_get_property_reply_t *reply = xcb_get_property_reply(c, cookie, nullptr);
    bool ok = false;
    if (reply && reply->format == 32 && reply->value_len > 0) {
        const uint32_t *data = static_cast<const uint32_t *>(xcb_get_property_value(reply));
        if (pidOut)
            *pidOut = static_cast<long>(data[0]);
        ok = true;
    }
    if (reply)
        free(reply);
    return ok;
}

bool X11::hasWmClass(xcb_window_t w) {
    xcb_connection_t *c = connection();
    if (!c)
        return false;
    xcb_get_property_cookie_t cookie =
        xcb_get_property(c, 0, w, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 0, 128);
    xcb_get_property_reply_t *reply = xcb_get_property_reply(c, cookie, nullptr);
    const bool has = reply && reply->value_len > 0;
    if (reply)
        free(reply);
    return has;
}

long X11::processGroupOf(long pid) {
    if (pid <= 0)
        return -1;
    QFile f(QStringLiteral("/proc/%1/stat").arg(pid));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return -1;
    const QString content = QString::fromLatin1(f.readAll());
    // comm may contain spaces and parens: skip to after the last ')'
    const int close = content.lastIndexOf(')');
    if (close < 0)
        return -1;
    const QStringList fields = content.mid(close + 2).split(' ', Qt::SkipEmptyParts);
    // fields now start at field 3 (state); pgrp is field 5 -> index 2
    if (fields.size() < 3)
        return -1;
    bool ok = false;
    const long pgrp = fields[2].toLong(&ok);
    return ok ? pgrp : -1;
}

QString X11::windowTitle(xcb_window_t w) {
    QString title = readTextProperty(w, intern("_NET_WM_NAME"), intern("UTF8_STRING"));
    if (title.isEmpty())
        title = readTextProperty(w, XCB_ATOM_WM_NAME, XCB_ATOM_STRING);
    return title;
}

bool X11::windowAlive(xcb_window_t w) {
    xcb_connection_t *c = connection();
    if (!c)
        return false;
    xcb_get_window_attributes_cookie_t cookie = xcb_get_window_attributes(c, w);
    xcb_get_window_attributes_reply_t *reply = xcb_get_window_attributes_reply(c, cookie, nullptr);
    const bool alive = reply != nullptr;
    if (reply)
        free(reply);
    return alive;
}

void X11::reparent(xcb_window_t child, xcb_window_t parent) {
    xcb_connection_t *c = connection();
    if (!c)
        return;
    xcb_reparent_window(c, child, parent, 0, 0);
}

void X11::moveResize(xcb_window_t w, int x, int y, int width, int height) {
    xcb_connection_t *c = connection();
    if (!c)
        return;
    const uint32_t values[] = {static_cast<uint32_t>(x), static_cast<uint32_t>(y),
                               static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    xcb_configure_window(c, w,
                         XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                             XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                         values);
}

void X11::mapWindow(xcb_window_t w) {
    if (xcb_connection_t *c = connection())
        xcb_map_window(c, w);
}

void X11::roundBottomCorners(xcb_window_t w, int width, int height, int radius) {
    xcb_connection_t *c = connection();
    if (!c || !xcb_get_extension_data(c, &xcb_shape_id) ||
        !xcb_get_extension_data(c, &xcb_shape_id)->present) {
        return;
    }
    if (width < 2 || height < 2)
        return;
    radius = qBound(0, radius, qMin(width, height) / 2);
    QVector<xcb_rectangle_t> rects;
    rects.reserve(height + radius);
    for (int y = 0; y < height; ++y) {
        int inset = 0;
        if (y >= height - radius && radius > 0) {
            const int dy = y - (height - radius);
            // quarter-circle inset: r - sqrt(r^2 - dy^2)
            const double val = radius * radius - double(dy) * dy;
            inset = radius - int(std::sqrt(qMax(0.0, val)));
        }
        xcb_rectangle_t r;
        r.x = static_cast<int16_t>(inset);
        r.y = static_cast<int16_t>(y);
        r.width = static_cast<uint16_t>(qMax(1, width - 2 * inset));
        r.height = 1;
        rects.append(r);
    }
    xcb_shape_rectangles(c, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING,
                         XCB_CLIP_ORDERING_UNSORTED, w, 0, 0,
                         static_cast<uint32_t>(rects.size()), rects.constData());
    // The window has an empty shape mask by default; also enable it.
    xcb_shape_mask(c, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING, w, 0, 0, XCB_PIXMAP_NONE);
}

void X11::unmapWindow(xcb_window_t w) {
    if (xcb_connection_t *c = connection())
        xcb_unmap_window(c, w);
}

void X11::raiseWindow(xcb_window_t w) {
    xcb_connection_t *c = connection();
    if (!c)
        return;
    const uint32_t value = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(c, w, XCB_CONFIG_WINDOW_STACK_MODE, &value);
}

void X11::setInputFocus(xcb_window_t w) {
    xcb_connection_t *c = connection();
    if (!c)
        return;
    xcb_set_input_focus(c, XCB_INPUT_FOCUS_POINTER_ROOT, w, XCB_CURRENT_TIME);
}

void X11::selectEmbedEvents(xcb_window_t w) {
    xcb_connection_t *c = connection();
    if (!c)
        return;
    const uint32_t mask = XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_FOCUS_CHANGE |
                          XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_change_window_attributes(c, w, XCB_CW_EVENT_MASK, &mask);
}

void X11::requestClose(xcb_window_t w) {
    xcb_connection_t *c = connection();
    if (!c)
        return;
    const xcb_atom_t wmProtocols = intern("WM_PROTOCOLS");
    const xcb_atom_t wmDelete = intern("WM_DELETE_WINDOW");

    // Only send WM_DELETE_WINDOW when the client supports it.
    bool supports = false;
    xcb_get_property_cookie_t cookie =
        xcb_get_property(c, 0, w, wmProtocols, XCB_ATOM_ATOM, 0, 64);
    xcb_get_property_reply_t *reply = xcb_get_property_reply(c, cookie, nullptr);
    if (reply) {
        xcb_atom_t *atoms = static_cast<xcb_atom_t *>(xcb_get_property_value(reply));
        const int n = reply->value_len / (reply->format / 8);
        for (int i = 0; i < n; ++i) {
            if (atoms[i] == wmDelete) {
                supports = true;
                break;
            }
        }
        free(reply);
    }

    if (!supports) {
        killClientConnection(w);
        return;
    }

    xcb_client_message_event_t event = {};
    event.response_type = XCB_CLIENT_MESSAGE;
    event.window = w;
    event.type = wmProtocols;
    event.format = 32;
    event.data.data32[0] = wmDelete;
    event.data.data32[1] = XCB_CURRENT_TIME;
    xcb_send_event(c, 0, w, XCB_EVENT_MASK_NO_EVENT, reinterpret_cast<const char *>(&event));
}

void X11::killClientConnection(xcb_window_t w) {
    if (xcb_connection_t *c = connection())
        xcb_kill_client(c, w);
}
