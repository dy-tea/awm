#include "Server.h"

XWaylandSurface::XWaylandSurface(
    struct wlr_xwayland_surface *xwayland_surface) {
    this->xwayland_surface = xwayland_surface;

    // destroy
    destroy.notify = [](struct wl_listener *listener, void *data) {
        XWaylandSurface *surface = wl_container_of(listener, surface, destroy);
        delete surface;
    };
    wl_signal_add(&xwayland_surface->events.destroy, &destroy);

    // request_configure
    request_configure.notify = [](struct wl_listener *listener, void *data) {
        XWaylandSurface *surface =
            wl_container_of(listener, surface, request_configure);
        wlr_xwayland_surface_configure_event *event =
            (wlr_xwayland_surface_configure_event *)data;

        wlr_xwayland_surface_configure(surface->xwayland_surface, event->x,
                                       event->y, event->width, event->height);
    };
    wl_signal_add(&xwayland_surface->events.request_configure,
                  &request_configure);

    // request_move
    request_move.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.request_move, &request_move);

    // request_resize
    request_resize.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.request_resize, &request_resize);

    // request_minimize
    request_minimize.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.request_minimize,
                  &request_minimize);

    // request_maximize
    request_maximize.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.request_maximize,
                  &request_maximize);

    // request_fullscreen
    request_fullscreen.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.request_fullscreen,
                  &request_fullscreen);

    // request_activate
    request_activate.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.request_activate,
                  &request_activate);

    // request_close
    request_close.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.request_close, &request_close);

    // request_sticky
    request_sticky.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.request_sticky, &request_sticky);

    // request_shaded
    request_shaded.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.request_shaded, &request_shaded);

    // request_skip_taskbar
    request_skip_taskbar.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.request_skip_taskbar,
                  &request_skip_taskbar);

    // request_skip_pager
    request_skip_pager.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.request_skip_pager,
                  &request_skip_pager);

    // request_above
    request_above.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.request_above, &request_above);

    // request_below
    request_below.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.request_below, &request_below);

    // request_demands_attention
    request_demands_attention.notify = [](struct wl_listener *listener,
                                          void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.request_demands_attention,
                  &request_demands_attention);

    // associate
    associate.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.associate, &associate);

    // dissociate
    dissociate.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.dissociate, &dissociate);

    // set_title
    set_title.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.set_title, &set_title);

    // set_class
    set_class.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.set_class, &set_class);

    // set_role
    set_role.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.set_role, &set_role);

    // set_parent
    set_parent.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.set_parent, &set_parent);

    // set_startup_id
    set_startup_id.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.set_startup_id, &set_startup_id);

    // set_window_type
    set_window_type.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.set_window_type, &set_window_type);

    // set_hints
    set_hints.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.set_hints, &set_hints);

    // set_decorations
    set_decorations.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.set_decorations, &set_decorations);

    // set_strut_partial
    set_strut_partial.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.set_strut_partial,
                  &set_strut_partial);

    // set_override_redirect
    set_override_redirect.notify = [](struct wl_listener *listener,
                                      void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.set_override_redirect,
                  &set_override_redirect);

    // set_geometry
    set_geometry.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.set_geometry, &set_geometry);

    // focus_in
    focus_in.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.focus_in, &focus_in);

    // grab_focus
    grab_focus.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.grab_focus, &grab_focus);

    // map_request
    map_request.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.map_request, &map_request);

    // ping_timeout
    ping_timeout.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->events.ping_timeout, &ping_timeout);
}

XWaylandSurface::~XWaylandSurface() {
    wl_list_remove(&destroy.link);
    wl_list_remove(&link);
    wl_list_remove(&request_configure.link);
    wl_list_remove(&request_move.link);
    wl_list_remove(&request_resize.link);
    wl_list_remove(&request_minimize.link);
    wl_list_remove(&request_maximize.link);
    wl_list_remove(&request_fullscreen.link);
    wl_list_remove(&request_activate.link);
    wl_list_remove(&request_close.link);
    wl_list_remove(&request_sticky.link);
    wl_list_remove(&request_shaded.link);
    wl_list_remove(&request_skip_taskbar.link);
    wl_list_remove(&request_skip_pager.link);
    wl_list_remove(&request_above.link);
    wl_list_remove(&request_below.link);
    wl_list_remove(&request_demands_attention.link);
    wl_list_remove(&associate.link);
    wl_list_remove(&dissociate.link);
    wl_list_remove(&set_title.link);
    wl_list_remove(&set_class.link);
    wl_list_remove(&set_role.link);
    wl_list_remove(&set_parent.link);
    wl_list_remove(&set_startup_id.link);
    wl_list_remove(&set_window_type.link);
    wl_list_remove(&set_hints.link);
    wl_list_remove(&set_decorations.link);
    wl_list_remove(&set_strut_partial.link);
    wl_list_remove(&set_override_redirect.link);
    wl_list_remove(&set_geometry.link);
    wl_list_remove(&focus_in.link);
    wl_list_remove(&grab_focus.link);
    wl_list_remove(&map_request.link);
    wl_list_remove(&ping_timeout.link);
}
