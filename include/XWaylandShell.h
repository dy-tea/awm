#include "XWaylandSurface.h"

struct XWaylandShell {
    struct wlr_xwayland_shell_v1 *xwayland_shell;
    struct wl_list surfaces;
    struct wlr_scene *scene;

    struct wl_listener destroy;
    struct wl_listener new_surface;

    XWaylandShell(struct wl_display *display, struct wlr_scene *scene);
    ~XWaylandShell();
};
