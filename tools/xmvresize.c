/* xmvresize: ask the WM (kwin) to resize a window to w x h.
 * Uses the _NET_MOVERESIZE_WINDOW client message with SIZE direction. */
#include <xcb/xcb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 4)
        return 2;
    xcb_connection_t *c = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(c))
        return 1;

    xcb_intern_atom_cookie_t ck = xcb_intern_atom(c, 0, 21, "_NET_MOVERESIZE_WINDOW");
    xcb_intern_atom_reply_t *r = xcb_intern_atom_reply(c, ck, NULL);
    xcb_atom_t atom = r ? r->atom : XCB_ATOM_NONE;
    free(r);
    if (atom == XCB_ATOM_NONE) {
        fprintf(stderr, "no _NET_MOVERESIZE_WINDOW\n");
        return 1;
    }

    xcb_window_t w = (xcb_window_t)strtoul(argv[1], NULL, 0);
    xcb_client_message_event_t ev = {};
    ev.response_type = XCB_CLIENT_MESSAGE;
    ev.window = w;
    ev.type = atom;
    ev.format = 32;
    ev.data.data32[0] = 100;                       // x
    ev.data.data32[1] = 100;                       // y
    ev.data.data32[2] = (uint32_t)atoi(argv[2]);   // width
    ev.data.data32[3] = (uint32_t)atoi(argv[3]);   // height
    ev.data.data32[4] = 8;                         // _NET_WM_MOVERESIZE_SIZE

    xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
    xcb_send_event(c, 0, screen->root,
                   XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,
                   (const char *)&ev);
    xcb_flush(c);
    xcb_disconnect(c);
    printf("move-resize request sent: %sx%s\n", argv[2], argv[3]);
    return 0;
}
