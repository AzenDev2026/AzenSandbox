/* xsweep: walk the entire X window tree on $DISPLAY and print every window's
 * id, WM_CLASS, name and _NET_WM_PID. With --kill, XKillClient every client
 * whose class or name matches the sandbox's apps (konsole, xmessage,
 * azen-sandbox) — the sandbox reparents windows, so top-level listings miss
 * them; this walks the full tree instead.
 */
#include <xcb/xcb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static xcb_connection_t *c;

static xcb_atom_t intern(const char *name) {
    xcb_intern_atom_cookie_t ck = xcb_intern_atom(c, 0, (uint16_t)strlen(name), name);
    xcb_intern_atom_reply_t *r = xcb_intern_atom_reply(c, ck, NULL);
    xcb_atom_t a = r ? r->atom : XCB_ATOM_NONE;
    free(r);
    return a;
}

static char *prop_bytes(xcb_window_t w, xcb_atom_t prop, xcb_atom_t type, int *len) {
    if (prop == XCB_ATOM_NONE)
        return NULL;
    xcb_get_property_cookie_t ck = xcb_get_property(c, 0, w, prop, type, 0, 1024);
    xcb_get_property_reply_t *r = xcb_get_property_reply(c, ck, NULL);
    if (!r || r->value_len == 0) {
        free(r);
        return NULL;
    }
    int n = r->value_len;
    char *buf = malloc(n + 1);
    memcpy(buf, xcb_get_property_value(r), n);
    buf[n] = 0;
    if (len)
        *len = n;
    free(r);
    return buf;
}

static void walk(xcb_window_t root, int depth, int kill) {
    static xcb_atom_t wm_class, net_name, wm_name, net_pid, utf8;
    static int atoms_init = 0;
    if (!atoms_init) {
        wm_class = intern("WM_CLASS");
        net_name = intern("_NET_WM_NAME");
        wm_name = intern("WM_NAME");
        net_pid = intern("_NET_WM_PID");
        utf8 = intern("UTF8_STRING");
        atoms_init = 1;
    }

    xcb_query_tree_cookie_t ck = xcb_query_tree(c, root);
    xcb_query_tree_reply_t *r = xcb_query_tree_reply(c, ck, NULL);
    if (!r)
        return;
    xcb_window_t *kids = xcb_query_tree_children(r);
    int n = xcb_query_tree_children_length(r);
    for (int i = 0; i < n; ++i) {
        xcb_window_t w = kids[i];
        char *cls = prop_bytes(w, wm_class, XCB_ATOM_STRING, NULL);
        char *name = prop_bytes(w, net_name, utf8, NULL);
        if (!name)
            name = prop_bytes(w, wm_name, XCB_ATOM_STRING, NULL);
        char *pid = prop_bytes(w, net_pid, XCB_ATOM_CARDINAL, NULL);

        printf("0x%x class=%s name=%s pid=%s\n", w,
               cls ? cls : "-", name ? name : "-", pid ? pid : "-");

        int hit = 0;
        if (kill) {
            const char *needles[] = {"konsole", "xmessage", "azen-sandbox", "azen sandbox"};
            for (int k = 0; k < 4; ++k) {
                if ((cls && strcasestr(cls, needles[k])) ||
                    (name && strcasestr(name, needles[k]))) {
                    hit = 1;
                    break;
                }
            }
            if (hit) {
                xcb_kill_client(c, w);
                printf("  -> XKillClient sent for 0x%x\n", w);
            }
        }
        free(cls);
        free(name);
        free(pid);
        walk(w, depth + 1, kill);
    }
    free(r);
}

int main(int argc, char **argv) {
    int kill = 0;
    if (argc > 1 && strcmp(argv[1], "--kill") == 0)
        kill = 1;
    c = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(c)) {
        fprintf(stderr, "cannot connect to X display\n");
        return 1;
    }
    xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
    walk(screen->root, 0, kill);
    xcb_flush(c);
    xcb_disconnect(c);
    return 0;
}
