#include "wlr.h"

struct XWaylandSurface {
    struct wl_list link;
    struct XWaylandShell *xwayland_shell;
    struct wlr_xwayland_surface_v1 *xwayland_surface;
    struct wlr_scene_surface *scene_surface;

    struct wl_listener client_commit;
    struct wl_listener commit;
    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener new_subsurface;
    struct wl_listener destroy;

    XWaylandSurface(struct XWaylandShell *xwayland_shell,
                    struct wlr_xwayland_surface_v1 *xwayland_surface);
    ~XWaylandSurface();
};
