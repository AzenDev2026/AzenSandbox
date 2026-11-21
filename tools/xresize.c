/* xresize: resize a top-level window on $DISPLAY. Usage: xresize <winid> <w> <h> */
#include <xcb/xcb.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 4)
        return 2;
    xcb_connection_t *c = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(c))
        return 1;
    xcb_window_t w = (xcb_window_t)strtoul(argv[1], NULL, 0);
    uint32_t vals[] = {(uint32_t)atoi(argv[2]), (uint32_t)atoi(argv[3])};
    xcb_configure_window(c, w, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, vals);
    xcb_flush(c);
    xcb_disconnect(c);
    printf("resized 0x%x to %sx%s\n", w, argv[2], argv[3]);
    return 0;
}
