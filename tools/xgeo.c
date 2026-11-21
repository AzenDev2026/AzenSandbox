/* xgeo: print a window's geometry. Usage: xgeo <winid> */
#include <xcb/xcb.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 2)
        return 2;
    xcb_connection_t *c = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(c))
        return 1;
    xcb_window_t w = (xcb_window_t)strtoul(argv[1], NULL, 0);
    xcb_get_geometry_cookie_t ck = xcb_get_geometry(c, w);
    xcb_get_geometry_reply_t *r = xcb_get_geometry_reply(c, ck, NULL);
    if (r)
        printf("0x%x geo: %ux%u+%d+%d\n", w, r->width, r->height, r->x, r->y);
    else
        printf("0x%x geo: gone\n", w);
    free(r);
    xcb_disconnect(c);
    return 0;
}
