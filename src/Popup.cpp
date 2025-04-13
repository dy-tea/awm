#include "Server.h"

Popup::Popup(wlr_xdg_popup *xdg_popup, wlr_scene_tree *parent_tree,
             Server *server)
    : server(server), xdg_popup(xdg_popup),
      parent_tree(wlr_scene_xdg_surface_create(parent_tree, xdg_popup->base)) {
    xdg_popup->base->data = parent_tree;

    // xdg_popup_commit
    commit.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        Popup *popup = wl_container_of(listener, popup, commit);
        Output *output = popup->server->focused_output();

        // output is required for popup positioning
        if (!output)
            return;

        // get the coords of the parent tree's scene node
        int lx, ly;
        wlr_scene_node_coords(&popup->parent_tree->node.parent->node, &lx, &ly);

        // calculate the geometry of the popup relative to the parent tree's
        // scene node
        wlr_box box = {
            output->layout_geometry.x - lx,
            output->layout_geometry.y - ly,
            output->layout_geometry.width,
            output->layout_geometry.height,
        };

        // unconstrain the popup from the relative box
        wlr_xdg_popup_unconstrain_from_box(popup->xdg_popup, &box);

        // schedule configure
        if (popup->xdg_popup->base->initial_commit)
            wlr_xdg_surface_schedule_configure(popup->xdg_popup->base);
    };
    wl_signal_add(&xdg_popup->base->surface->events.commit, &commit);

    // xdg_popup_destroy
    destroy.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        Popup *popup = wl_container_of(listener, popup, destroy);
        delete popup;
    };
    wl_signal_add(&xdg_popup->events.destroy, &destroy);

    // xdg_popup_popup
    new_popup.notify = [](wl_listener *listener, void *data) {
        Popup *popup = wl_container_of(listener, popup, new_popup);
        auto *xdg_popup = static_cast<wlr_xdg_popup *>(data);

        new Popup(xdg_popup, popup->parent_tree, popup->server);
    };
    wl_signal_add(&xdg_popup->base->events.new_popup, &new_popup);
}

Popup::~Popup() {
    wl_list_remove(&commit.link);
    wl_list_remove(&destroy.link);
    wl_list_remove(&new_popup.link);
}
