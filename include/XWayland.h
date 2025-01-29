#include "XWaylandSurface.h"

enum AtomName {
    NET_WM_WINDOW_TYPE_NORMAL,
    NET_WM_WINDOW_TYPE_DIALOG,
    NET_WM_WINDOW_TYPE_UTILITY,
    NET_WM_WINDOW_TYPE_TOOLBAR,
    NET_WM_WINDOW_TYPE_SPLASH,
    NET_WM_WINDOW_TYPE_MENU,
    NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
    NET_WM_WINDOW_TYPE_POPUP_MENU,
    NET_WM_WINDOW_TYPE_TOOLTIP,
    NET_WM_WINDOW_TYPE_NOTIFICATION,
    NET_WM_STATE_MODAL,
    ATOM_LAST,
};

struct XWayland {
    struct wlr_xwayland *xwayland;
    xcb_atom_t atoms[ATOM_LAST];
    struct wl_list surfaces;

    struct wl_listener destroy;
    struct wl_listener ready;
    struct wl_listener new_surface;
    struct wl_listener remove_startup_info;

    XWayland(struct wl_display *display, struct wlr_compositor *compositor);
    ~XWayland();
};
