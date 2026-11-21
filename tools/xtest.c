/* xtest: simulate pointer input on $DISPLAY.
 *   xtest click <x> <y>
 *   xtest drag <x1> <y1> <x2> <y2>
 */
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void click(Display *d, int x, int y) {
    XTestFakeMotionEvent(d, -1, x, y, CurrentTime);
    XTestFakeButtonEvent(d, 1, True, CurrentTime);
    XTestFakeButtonEvent(d, 1, False, CurrentTime);
    XFlush(d);
}

static void drag(Display *d, int x1, int y1, int x2, int y2) {
    XTestFakeMotionEvent(d, -1, x1, y1, CurrentTime);
    XTestFakeButtonEvent(d, 1, True, CurrentTime);
    XFlush(d);
    usleep(150000);
    const int steps = 12;
    for (int i = 1; i <= steps; ++i) {
        XTestFakeMotionEvent(d, -1, x1 + (x2 - x1) * i / steps,
                             y1 + (y2 - y1) * i / steps, CurrentTime);
        XFlush(d);
        usleep(30000);
    }
    usleep(150000);
    XTestFakeButtonEvent(d, 1, False, CurrentTime);
    XFlush(d);
}

int main(int argc, char **argv) {
    if (argc < 4)
        return 2;
    Display *d = XOpenDisplay(NULL);
    if (!d) {
        fprintf(stderr, "no display\n");
        return 1;
    }
    if (strcmp(argv[1], "click") == 0) {
        click(d, atoi(argv[2]), atoi(argv[3]));
        printf("clicked %d,%d\n", atoi(argv[2]), atoi(argv[3]));
    } else if (strcmp(argv[1], "drag") == 0 && argc >= 6) {
        drag(d, atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));
        printf("dragged %d,%d -> %d,%d\n", atoi(argv[2]), atoi(argv[3]),
               atoi(argv[4]), atoi(argv[5]));
    }
    XCloseDisplay(d);
    return 0;
}
