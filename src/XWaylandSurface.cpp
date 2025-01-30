#include "Server.h"
#include "wlr.h"

XWaylandSurface::XWaylandSurface(
    struct XWaylandShell *xwayland_shell,
    struct wlr_xwayland_surface_v1 *xwayland_surface) {
    this->xwayland_shell = xwayland_shell;
    this->xwayland_surface = xwayland_surface;
    scene_surface = wlr_scene_surface_create(&xwayland_shell->scene->tree,
                                             xwayland_surface->surface);

    // client_commit
    client_commit.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->surface->events.client_commit,
                  &client_commit);

    // commit
    commit.notify = [](struct wl_listener *listener, void *data) {

    };
    wl_signal_add(&xwayland_surface->surface->events.commit, &commit);

    // map
    map.notify = [](struct wl_listener *listener, void *data) {
        XWaylandSurface *surface = wl_container_of(listener, surface, map);
        wlr_scene_node_set_enabled(&surface->xwayland_shell->scene->tree.node,
                                   true);
    };
    wl_signal_add(&xwayland_surface->surface->events.map, &map);

    // unmap
    unmap.notify = [](struct wl_listener *listener, void *data) {
        XWaylandSurface *surface = wl_container_of(listener, surface, map);
        wlr_scene_node_set_enabled(&surface->xwayland_shell->scene->tree.node,
                                   false);
    };
    wl_signal_add(&xwayland_surface->surface->events.unmap, &unmap);

    // new_subsurface
    new_subsurface.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO");
    };
    wl_signal_add(&xwayland_surface->surface->events.new_subsurface,
                  &new_subsurface);

    // destroy
    destroy.notify = [](struct wl_listener *listener, void *data) {
        XWaylandSurface *surface = wl_container_of(listener, surface, destroy);
        delete surface;
    };
    wl_signal_add(&xwayland_surface->surface->events.destroy, &destroy);
}

XWaylandSurface::~XWaylandSurface() {
    wl_list_remove(&client_commit.link);
    wl_list_remove(&commit.link);
    wl_list_remove(&map.link);
    wl_list_remove(&unmap.link);
    wl_list_remove(&new_subsurface.link);
    wl_list_remove(&destroy.link);
}
