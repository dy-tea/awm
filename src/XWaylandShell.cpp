#include "XWaylandShell.h"

XWaylandShell::XWaylandShell(struct wl_display *display) {
    xwayland_shell = wlr_xwayland_shell_v1_create(display, 1);
    if (xwayland_shell == nullptr) {
        wlr_log(WLR_ERROR, "Failed to create XWayland shell");
        return;
    }

    wl_list_init(&surfaces);

    // destroy
    destroy.notify = [](struct wl_listener *listener, void *data) {
        struct XWaylandShell *shell = wl_container_of(listener, shell, destroy);
        delete shell;
    };
    wl_signal_add(&xwayland_shell->events.destroy, &destroy);

    // new_surface
    new_surface.notify = [](struct wl_listener *listener, void *data) {
        XWaylandShell *shell = wl_container_of(listener, shell, new_surface);
        XWaylandSurface *surface =
            new XWaylandSurface((wlr_xwayland_surface *)data);
        wl_list_insert(&shell->surfaces, &surface->link);
    };
    wl_signal_add(&xwayland_shell->events.new_surface, &new_surface);
}

XWaylandShell::~XWaylandShell() {
    wl_list_remove(&destroy.link);
    wl_list_remove(&new_surface.link);

    struct XWaylandSurface *cur, *tmp;
    wl_list_for_each_safe(cur, tmp, &surfaces, link) delete cur;

    wlr_xwayland_shell_v1_destroy(xwayland_shell);
}
