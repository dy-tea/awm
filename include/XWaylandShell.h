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

struct XWaylandShell {
    struct wlr_xwayland_shell_v1 *xwayland_shell;
    struct wl_list surfaces;

    struct wl_listener destroy;
    struct wl_listener new_surface;

    XWaylandShell(struct wl_display *display);
    ~XWaylandShell();
};
