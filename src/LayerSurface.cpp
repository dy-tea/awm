#include "Server.h"

LayerSurface::LayerSurface(struct Output *output,
                           struct wlr_layer_surface_v1 *wlr_layer_surface) {
    this->output = output;
    this->wlr_layer_surface = wlr_layer_surface;
    wl_list_init(&popups);

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
        wlr_scene_node_set_enabled(&surface->scene_layer_surface->tree->node,
                                   true);

        // rearrange
        surface->output->arrange_layers();

        // handle focus
        if (surface->wlr_layer_surface->current.keyboard_interactive)
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

        // get associated output
        Output *output = surface->output;

        // layer changed reparent to new layer tree
        bool needs_arrange = false;
        if (surface->wlr_layer_surface->current.committed &
            WLR_LAYER_SURFACE_V1_STATE_LAYER) {
            struct wlr_scene_tree *new_tree =
                output->shell_layer(surface->wlr_layer_surface->current.layer);
            wlr_scene_node_reparent(&surface->scene_layer_surface->tree->node,
                                    new_tree);
            wlr_scene_node_raise_to_top(
                &surface->scene_layer_surface->tree->node);
            needs_arrange = true;
        }

        wlr_layer_surface_v1 *layer_surface = surface->wlr_layer_surface;

        // get usable area of the output
        struct wlr_box usable_area = output->usable_area;

        // get output box
        struct wlr_box output_box;
        wlr_output_layout_get_box(output->server->output_layout,
                                  output->wlr_output, &output_box);

        // set default location to center of screen
        if (!layer_surface->configured) {
            // get the actual dimensions of the surface
            uint32_t width = layer_surface->current.actual_width;
            uint32_t height = layer_surface->current.actual_height;

            // calculate target position based on anchor points
            int32_t x = output_box.x;
            int32_t y = output_box.y;

            // center horizontally if not anchored horizontally
            if (!(layer_surface->current.anchor &
                  (ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                   ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT)))
                x += (usable_area.width - width) / 2;

            // center vertically if not anchored vertically
            if (!(layer_surface->current.anchor &
                  (ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                   ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM)))
                y += (usable_area.height - height) / 2;

            // set the postion
            wlr_scene_node_set_position(
                &surface->scene_layer_surface->tree->node, x, y);

            // get desired size
            width = layer_surface->current.desired_width;
            height = layer_surface->current.desired_height;

            if (!width)
                width = output_box.width;
            if (!height)
                height = output_box.height;

            wlr_log(WLR_INFO,
                    "layer surface configured to %dx%d at position %d, %d",
                    width, height, x, y);

            // send configure with size
            wlr_layer_surface_v1_configure(layer_surface, width, height);
        }

        // rearrange if needed
        if (needs_arrange)
            output->arrange_layers();
    };
    wl_signal_add(&wlr_layer_surface->surface->events.commit, &commit);

    // new_popup
    new_popup.notify = [](struct wl_listener *listener, void *data) {
        LayerSurface *layer_surface =
            wl_container_of(listener, layer_surface, new_popup);

        // add to list of popups
        struct Popup *popup = new Popup((wlr_xdg_popup *)data);
        if (popup)
            wl_list_insert(&layer_surface->popups, &popup->link);
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

    // remove associated popups
    struct Popup *popup, *tmp;
    wl_list_for_each_safe(popup, tmp, &popups, link) delete popup;

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
