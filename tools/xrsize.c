/* xrsize: resize the X server screen via RANDR. Usage: xrsize <w> <h> */
#include <xcb/xcb.h>
#include <xcb/randr.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 3)
        return 2;
    xcb_connection_t *c = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(c))
        return 1;
    xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
    xcb_randr_set_screen_size(c, screen->root, (uint16_t)atoi(argv[1]),
                              (uint16_t)atoi(argv[2]), 0, 0);
    xcb_flush(c);
    xcb_disconnect(c);
    printf("randr screen -> %sx%s\n", argv[1], argv[2]);
    return 0;
}
