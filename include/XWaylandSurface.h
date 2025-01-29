#include "wlr.h"

struct XWaylandSurface {
    struct wl_list link;
    struct wlr_xwayland_surface *xwayland_surface;

    struct wl_listener destroy;
    struct wl_listener
        request_configure; // struct wlr_xwayland_surface_configure_event
    struct wl_listener request_move;
    struct wl_listener request_resize;   // struct wlr_xwayland_resize_event
    struct wl_listener request_minimize; // struct wlr_xwayland_minimize_event
    struct wl_listener request_maximize;
    struct wl_listener request_fullscreen;
    struct wl_listener request_activate;
    struct wl_listener request_close;
    struct wl_listener request_sticky;
    struct wl_listener request_shaded;
    struct wl_listener request_skip_taskbar;
    struct wl_listener request_skip_pager;
    struct wl_listener request_above;
    struct wl_listener request_below;
    struct wl_listener request_demands_attention;

    struct wl_listener associate;
    struct wl_listener dissociate;

    struct wl_listener set_title;
    struct wl_listener set_class;
    struct wl_listener set_role;
    struct wl_listener set_parent;
    struct wl_listener set_startup_id;
    struct wl_listener set_window_type;
    struct wl_listener set_hints;
    struct wl_listener set_decorations;
    struct wl_listener set_strut_partial;
    struct wl_listener set_override_redirect;
    struct wl_listener set_geometry;
    struct wl_listener focus_in;
    struct wl_listener grab_focus;

    struct wl_listener map_request;
    struct wl_listener ping_timeout;

    XWaylandSurface(struct wlr_xwayland_surface *xwayland_surface);
    ~XWaylandSurface();
};
