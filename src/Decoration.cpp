#include "Server.h"

const int TITLEBAR_HEIGHT = 30;
const float TITLEBAR_COLOR[4] = {1.0f, 0.0f, 1.0f, 1.0f};

void Decoration::create_titlebar(int width) {
    if (titlebar)
        wlr_scene_node_destroy(&titlebar->node);

    titlebar = wlr_scene_rect_create(toplevel->scene_tree, width,
                                     TITLEBAR_HEIGHT, TITLEBAR_COLOR);

    wlr_log(WLR_INFO, "Titlebar dimensions: %d x %d", titlebar->width,
            titlebar->height);

    wlr_scene_node_set_position(&titlebar->node, 0, -TITLEBAR_HEIGHT);
}

Decoration::Decoration(Server *server,
                       wlr_xdg_toplevel_decoration_v1 *decoration)
    : server(server),
      toplevel(static_cast<Toplevel *>(decoration->toplevel->base->data)),
      decoration(decoration) {

    wl_list_insert(&server->decorations, &link);
    toplevel->decoration = this;

    commit.notify = nullptr;

    // mode
    mode.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        Decoration *deco = wl_container_of(listener, deco, mode);
        wlr_xdg_toplevel_decoration_v1_mode client_mode =
            deco->decoration->requested_mode;

        // prefer client side, could be config option
        deco->client_mode =
            client_mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_NONE
                ? WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE
                : client_mode;

        if (deco->decoration->toplevel->base->initialized)
            wlr_xdg_toplevel_decoration_v1_set_mode(deco->decoration,
                                                    client_mode);
        else if (!deco->commit.notify) {
            deco->commit.notify = [](wl_listener *listener,
                                     [[maybe_unused]] void *data) {
                Decoration *deco = wl_container_of(listener, deco, commit);
                wlr_xdg_toplevel_decoration_v1 *decoration = deco->decoration;

                if (decoration->toplevel->base->initial_commit) {
                    wlr_xdg_toplevel_decoration_v1_set_mode(decoration,
                                                            deco->client_mode);
                    wl_list_remove(&deco->commit.link);
                    deco->commit.notify = nullptr;
                }

                Toplevel *toplevel = deco->toplevel;

                if (toplevel->decoration_mode ==
                    WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE)
                    deco->create_titlebar(toplevel->geometry.width);
            };
            wl_signal_add(
                &deco->decoration->toplevel->base->surface->events.commit,
                &deco->commit);
        }

        deco->toplevel->set_decoration_mode(client_mode);
    };
    wl_signal_add(&decoration->events.request_mode, &mode);

    // destroy
    destroy.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        Decoration *deco = wl_container_of(listener, deco, destroy);
        delete deco;
    };
    wl_signal_add(&decoration->events.destroy, &destroy);
}

Decoration::~Decoration() {
    if (toplevel)
        toplevel->decoration = nullptr;

    wl_list_remove(&mode.link);
    wl_list_remove(&destroy.link);
    wl_list_remove(&link);

    if (commit.notify) {
        wl_list_remove(&commit.link);
        commit.notify = nullptr;
    }
}
