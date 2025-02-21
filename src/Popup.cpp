#include "Server.h"

Popup::Popup(struct wlr_xdg_popup *xdg_popup, struct Server *server) {
    this->xdg_popup = xdg_popup;
    this->server = server;

    // we need a parent to ascertain the type
    if (!xdg_popup->parent)
        return;

    // get parent scene tree
    struct wlr_scene_tree *parent_tree = NULL;

    // check if parent is layer surface
    struct wlr_layer_surface_v1 *layer =
        wlr_layer_surface_v1_try_from_wlr_surface(xdg_popup->parent);

    if (layer) {
        // parent tree is layer surface
        LayerSurface *layer_surface = static_cast<LayerSurface *>(layer->data);
        parent_tree = layer_surface->scene_layer_surface->tree;
    } else {
        // parent tree is xdg surface
        struct wlr_xdg_surface *parent =
            wlr_xdg_surface_try_from_wlr_surface(xdg_popup->parent);
        if (parent)
            parent_tree = static_cast<wlr_scene_tree *>(parent->data);
        else {
            wlr_log(WLR_ERROR, "failed to get parent tree");
            return;
        }
    }

    // create scene node for popup
    xdg_popup->base->data =
        wlr_scene_xdg_surface_create(parent_tree, xdg_popup->base);

    // xdg_popup_commit
    commit.notify = [](struct wl_listener *listener, void *data) {
        Popup *popup = wl_container_of(listener, popup, commit);

        struct wlr_surface *surface = static_cast<wlr_surface *>(data);
        struct wlr_xdg_popup *xdg_popup =
            wlr_xdg_popup_try_from_wlr_surface(surface);

        if (!xdg_popup->base->initial_commit)
            return;

        Output *output = popup->server->focused_output();
        if (output) {
            struct wlr_box box = output->usable_area;
            wlr_xdg_popup_unconstrain_from_box(xdg_popup, &box);
        }
    };
    wl_signal_add(&xdg_popup->base->surface->events.commit, &commit);

    // xdg_popup_destroy
    destroy.notify = [](struct wl_listener *listener, void *data) {
        struct Popup *popup = wl_container_of(listener, popup, destroy);
        delete popup;
    };
    wl_signal_add(&xdg_popup->events.destroy, &destroy);
}

Popup::~Popup() {
    wl_list_remove(&commit.link);
    wl_list_remove(&destroy.link);
}
