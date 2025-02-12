#include "Server.h"

LayerSurface::LayerSurface(struct Output *output,
                           struct wlr_layer_surface_v1 *wlr_layer_surface) {
    this->output = output;
    this->wlr_layer_surface = wlr_layer_surface;

    // create scene layer for surface
    struct wlr_scene_tree *layer_tree =
        output->shell_layer(wlr_layer_surface->pending.layer);
    scene_layer_surface =
        wlr_scene_layer_surface_v1_create(layer_tree, wlr_layer_surface);

    if (!scene_layer_surface) {
        wlr_log(WLR_ERROR, "failed to create scene layer surface");
        return;
    }

    // point this surface to data for later
    scene_layer_surface->tree->node.data = this;
    wlr_layer_surface->data = this;

    // map surface
    map.notify = [](struct wl_listener *listener, void *data) {
        // get seat and pointer focus on map
        LayerSurface *surface = wl_container_of(listener, surface, map);
        wlr_layer_surface_v1 *layer_surface = surface->wlr_layer_surface;
        wlr_scene_node_set_enabled(&surface->scene_layer_surface->tree->node,
                                   true);

        // rearrange
        surface->output->arrange_layers();

        // handle focus
        if (surface->wlr_layer_surface->current.keyboard_interactive &&
            (layer_surface->current.layer == ZWLR_LAYER_SHELL_V1_LAYER_TOP ||
             layer_surface->current.layer == ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY))
            surface->handle_focus();
    };
    wl_signal_add(&wlr_layer_surface->surface->events.map, &map);

    // unmap surface
    unmap.notify = [](struct wl_listener *listener, void *data) {
        LayerSurface *surface = wl_container_of(listener, surface, unmap);

        // disable surface
        wlr_scene_node_set_enabled(&surface->scene_layer_surface->tree->node,
                                   false);

        // arrange layers
        surface->output->arrange_layers();
    };
    wl_signal_add(&wlr_layer_surface->surface->events.unmap, &unmap);

    // commit surface
    commit.notify = [](struct wl_listener *listener, void *data) {
        // on display
        LayerSurface *surface = wl_container_of(listener, surface, commit);
        wlr_layer_surface_v1 *layer_surface = surface->wlr_layer_surface;

        if (!layer_surface->initialized)
            return;

        // get associated output
        Output *output = surface->output;

        bool needs_arrange = false;
        if (surface->wlr_layer_surface->current.committed &
            WLR_LAYER_SURFACE_V1_STATE_LAYER) {
            struct wlr_scene_tree *new_tree =
                output->shell_layer(surface->wlr_layer_surface->current.layer);
            wlr_scene_node_reparent(&surface->scene_layer_surface->tree->node,
                                    new_tree);
            needs_arrange = true;
        }

        // get output box
        struct wlr_box output_box;
        wlr_output_layout_get_box(output->server->output_layout,
                                  output->wlr_output, &output_box);

        // set default location to center of screen
        if (layer_surface->initial_commit) {
            // get the actual dimensions of the surface
            uint32_t width = layer_surface->current.desired_width;
            uint32_t height = layer_surface->current.desired_height;

            if (!width)
                width = output_box.width;
            if (!height)
                height = output_box.height;

            // send configure with size
            wlr_layer_surface_v1_configure(layer_surface, width, height);
        }

        // rearrange if needed
        if (needs_arrange || layer_surface->initial_commit ||
            layer_surface->current.committed)
            output->arrange_layers();
    };
    wl_signal_add(&wlr_layer_surface->surface->events.commit, &commit);

    // new_popup
    new_popup.notify = [](struct wl_listener *listener, void *data) {
        LayerSurface *layer_surface =
            wl_container_of(listener, layer_surface, new_popup);

        if (data) [[maybe_unused]]
            struct Popup *popup = new Popup((wlr_xdg_popup *)data);
    };
    wl_signal_add(&wlr_layer_surface->events.new_popup, &new_popup);

    // destroy
    destroy.notify = [](struct wl_listener *listener, void *data) {
        LayerSurface *surface = wl_container_of(listener, surface, destroy);
        delete surface;
    };
    wl_signal_add(&wlr_layer_surface->events.destroy, &destroy);
}

LayerSurface::~LayerSurface() {
    // rearrange on destroy
    output->arrange_layers();

    // remove links
    wl_list_remove(&link);
    wl_list_remove(&map.link);
    wl_list_remove(&unmap.link);
    wl_list_remove(&commit.link);
    wl_list_remove(&new_popup.link);
    wl_list_remove(&destroy.link);
}

// handle keyboard focus for layer shells
void LayerSurface::handle_focus() {
    if (!wlr_layer_surface || !wlr_layer_surface->surface ||
        !wlr_layer_surface->surface->mapped || !scene_layer_surface)
        return;

    if (wlr_layer_surface->current.keyboard_interactive ==
        ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE)
        return;

    struct wlr_surface *surface = wlr_layer_surface->surface;
    struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(output->server->seat);

    if (keyboard)
        wlr_seat_keyboard_notify_enter(
            output->server->seat, surface, keyboard->keycodes,
            keyboard->num_keycodes, &keyboard->modifiers);
}

// returns true if layer surface should be focusable
bool LayerSurface::should_focus() {
    if (!wlr_layer_surface || !wlr_layer_surface->surface ||
        !wlr_layer_surface->surface->mapped)
        return false;

    return wlr_layer_surface->current.keyboard_interactive !=
           ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE;
}
