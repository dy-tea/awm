#include "Server.h"

static void handle_mode(wl_listener *listener, [[maybe_unused]] void *data) {
    ServerDecoration *sd = wl_container_of(listener, sd, mode);
    Toplevel *toplevel = sd->server->get_toplevel(sd->decoration->surface);

    if (!toplevel)
        return;

    toplevel->set_ssd_mode(
        static_cast<wlr_server_decoration_manager_mode>(sd->decoration->mode));
}

ServerDecoration::ServerDecoration(Server *server,
                                   wlr_server_decoration *decoration)
    : server(server), decoration(decoration) {

    // cursed af code
    if (decoration->surface)
        if (wlr_xdg_surface *xdg_surface =
                wlr_xdg_surface_try_from_wlr_surface(decoration->surface);
            xdg_surface && xdg_surface->data)
            handle_mode((wl_listener *)this, (void *)decoration);

    // mode
    mode.notify = handle_mode;
    wl_signal_add(&decoration->events.mode, &mode);

    // destroy
    destroy.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        ServerDecoration *sd = wl_container_of(listener, sd, mode);
        delete sd;
    };
    wl_signal_add(&decoration->events.destroy, &destroy);
}

ServerDecoration::~ServerDecoration() {
    wl_list_remove(&mode.link);
    wl_list_remove(&destroy.link);
    wl_list_remove(&link);
}
