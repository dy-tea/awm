#include "Popup.h"
#include "Output.h"
#include "OutputManager.h"

Popup::Popup(wlr_xdg_popup *xdg_popup, wlr_scene_tree *parent_tree,
             wlr_scene_tree *image_capture_parent, Server *server)
    : server(server), xdg_popup(xdg_popup),
      parent_tree(wlr_scene_xdg_surface_create(parent_tree, xdg_popup->base)) {
    xdg_popup->base->data = parent_tree;

    // set capture parent if provided
    if (image_capture_parent) {
        image_capture_tree = wlr_scene_tree_create(image_capture_parent);

        // manually create surface for image capture
        wlr_scene_surface_create(image_capture_tree, xdg_popup->base->surface);
    }

    // xdg_popup_commit
    commit.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        Popup *popup = wl_container_of(listener, popup, commit);

        // only position on initial commit
        if (!popup->xdg_popup->base->initial_commit)
            return;

        popup->unconstrain();
    };
    wl_signal_add(&xdg_popup->base->surface->events.commit, &commit);

    // xdg_popup_reposition
    reposition.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        Popup *popup = wl_container_of(listener, popup, reposition);
        popup->unconstrain();
    };
    wl_signal_add(&xdg_popup->events.reposition, &reposition);

    // xdg_popup_new_popup
    new_popup.notify = [](wl_listener *listener, void *data) {
        Popup *popup = wl_container_of(listener, popup, new_popup);
        auto *xdg_popup = static_cast<wlr_xdg_popup *>(data);

        new Popup(xdg_popup, popup->parent_tree, popup->image_capture_tree,
                  popup->server);
    };
    wl_signal_add(&xdg_popup->base->events.new_popup, &new_popup);

    // xdg_popup_destroy
    destroy.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        Popup *popup = wl_container_of(listener, popup, destroy);
        delete popup;
    };
    wl_signal_add(&xdg_popup->events.destroy, &destroy);
}

Popup::~Popup() {
    wl_list_remove(&commit.link);
    wl_list_remove(&reposition.link);
    wl_list_remove(&new_popup.link);
    wl_list_remove(&destroy.link);
}

void Popup::unconstrain() {
    // get the coords of the parent tree's scene node
    int lx, ly;
    wlr_scene_node_coords(&parent_tree->node.parent->node, &lx, &ly);

    // get output containing parent tree's scene node
    Output *output = server->output_manager->output_at(lx, ly);
    if (!output)
        return;

    // calculate the geometry of the popup relative to the parent tree's
    // scene node
    wlr_box box = {
        output->layout_geometry.x - lx,
        output->layout_geometry.y - ly,
        output->layout_geometry.width,
        output->layout_geometry.height,
    };

    // unconstrain the popup from the relative box
    wlr_xdg_popup_unconstrain_from_box(xdg_popup, &box);
}
