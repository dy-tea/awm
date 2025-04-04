#include "Server.h"

ServerDecoration::ServerDecoration(Server *server,
                                   wlr_server_decoration *decoration)
    : server(server), decoration(decoration) {

    // mode
    mode.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        ServerDecoration *sd = wl_container_of(listener, sd, mode);
        Toplevel *toplevel = sd->server->get_toplevel(sd->decoration->surface);

        if (!toplevel)
            return;

        // update csd state
        toplevel->using_csd =
            sd->decoration->mode == WLR_SERVER_DECORATION_MANAGER_MODE_CLIENT;
        toplevel->request_decoration_mode(listener, toplevel->xdg_decoration);
    };
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
