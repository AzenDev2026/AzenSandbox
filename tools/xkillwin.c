/* xkillwin: force-kill an X11 client by window id (XKillClient).
 * Used from the helper environment to clean up stray sandbox clients on the
 * shared X display, whose processes live in another PID namespace.
 * Usage: xkillwin <winid> [<winid> ...]
 */
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: xkillwin <winid>...\n");
        return 2;
    }
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "cannot open display '%s'\n", getenv("DISPLAY") ? getenv("DISPLAY") : "(none)");
        return 1;
    }
    int killed = 0;
    for (int i = 1; i < argc; ++i) {
        Window w = (Window)strtoul(argv[i], NULL, 0);
        if (!w)
            continue;
        XKillClient(dpy, w);
        ++killed;
    }
    XSync(dpy, False);
    XCloseDisplay(dpy);
    printf("sent kill to %d client(s)\n", killed);
    return 0;
}
