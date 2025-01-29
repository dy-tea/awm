#include "XWayland.h"
#include <map>
#include <string>

static const std::map<AtomName, std::string> atom_map = {
    {NET_WM_WINDOW_TYPE_NORMAL, "_NET_WM_WINDOW_TYPE_NORMAL"},
    {NET_WM_WINDOW_TYPE_DIALOG, "_NET_WM_WINDOW_TYPE_DIALOG"},
    {NET_WM_WINDOW_TYPE_UTILITY, "_NET_WM_WINDOW_TYPE_UTILITY"},
    {NET_WM_WINDOW_TYPE_TOOLBAR, "_NET_WM_WINDOW_TYPE_TOOLBAR"},
    {NET_WM_WINDOW_TYPE_SPLASH, "_NET_WM_WINDOW_TYPE_SPLASH"},
    {NET_WM_WINDOW_TYPE_MENU, "_NET_WM_WINDOW_TYPE_MENU"},
    {NET_WM_WINDOW_TYPE_DROPDOWN_MENU, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"},
    {NET_WM_WINDOW_TYPE_POPUP_MENU, "_NET_WM_WINDOW_TYPE_POPUP_MENU"},
    {NET_WM_WINDOW_TYPE_TOOLTIP, "_NET_WM_WINDOW_TYPE_TOOLTIP"},
    {NET_WM_WINDOW_TYPE_NOTIFICATION, "_NET_WM_WINDOW_TYPE_NOTIFICATION"},
    {NET_WM_STATE_MODAL, "_NET_WM_STATE_MODAL"}};

XWayland::XWayland(struct wl_display *display,
                   struct wlr_compositor *compositor) {
    xwayland = wlr_xwayland_create(display, compositor, true);
    if (xwayland == nullptr) {
        wlr_log(WLR_ERROR, "Failed to create XWayland instance");
        return;
    }
    wl_list_init(&surfaces);

    // destroy
    destroy.notify = [](struct wl_listener *listener, void *data) {
        struct XWayland *xwayland =
            wl_container_of(listener, xwayland, destroy);
        delete xwayland;
    };
    wl_signal_add(&xwayland->events.destroy, &destroy);

    // ready
    ready.notify = [](struct wl_listener *listener, void *data) {
        struct XWayland *xwayland = wl_container_of(listener, xwayland, ready);

        xcb_connection_t *xcb_conn = xcb_connect(NULL, NULL);

        int err = xcb_connection_has_error(xcb_conn);
        if (err) {
            wlr_log(WLR_ERROR, "XCB connect failed: %d", err);
            return;
        }

        xcb_intern_atom_cookie_t cookies[ATOM_LAST];
        for (size_t i = 0; i < ATOM_LAST; i++) {
            const std::string name = atom_map.at((AtomName)i);
            cookies[i] =
                xcb_intern_atom(xcb_conn, 0, name.size(), name.c_str());
        }
        for (size_t i = 0; i < ATOM_LAST; i++) {
            xcb_generic_error_t *error = NULL;
            xcb_intern_atom_reply_t *reply =
                xcb_intern_atom_reply(xcb_conn, cookies[i], &error);
            if (reply != NULL && error == NULL) {
                xwayland->atoms[i] = reply->atom;
            }
            free(reply);

            if (error != NULL) {
                wlr_log(WLR_ERROR,
                        "could not resolve atom %s, X11 error code %d",
                        atom_map.at((AtomName)i).c_str(), error->error_code);
                free(error);
                break;
            }
        }

        xcb_disconnect(xcb_conn);
    };
    wl_signal_add(&xwayland->events.ready, &ready);

    // new_surface
    new_surface.notify = [](struct wl_listener *listener, void *data) {
        XWayland *xwayland = wl_container_of(listener, xwayland, new_surface);
        XWaylandSurface *surface =
            new XWaylandSurface((wlr_xwayland_surface *)data);
        wl_list_insert(&xwayland->surfaces, &surface->link);
    };
    wl_signal_add(&xwayland->events.new_surface, &new_surface);

    // remove_startup_info
    remove_startup_info.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_ERROR, "TODO: remove_startup_info");
    };
    wl_signal_add(&xwayland->events.remove_startup_info, &remove_startup_info);
}

XWayland::~XWayland() {
    wl_list_remove(&destroy.link);
    wl_list_remove(&ready.link);
    wl_list_remove(&new_surface.link);
    wl_list_remove(&remove_startup_info.link);

    struct XWaylandSurface *cur, *tmp;
    wl_list_for_each_safe(cur, tmp, &surfaces, link) delete cur;
}
