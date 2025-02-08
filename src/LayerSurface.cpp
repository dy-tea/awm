#include "Server.h"

LayerSurface::LayerSurface(struct LayerShell *shell,
                           struct wlr_layer_surface_v1 *wlr_layer_surface) {
    layer_shell = shell;
    this->wlr_layer_surface = wlr_layer_surface;
    wl_list_init(&popups);

    // layer shells must have an output
    if (!wlr_layer_surface->output) {
        wlr_log(WLR_ERROR, "layer surface created without an output");
        return;
    }

    Output *output = (Output *)wlr_layer_surface->output->data;
    if (!output) {
        wlr_log(WLR_ERROR, "layer surface has no associated Output instance");
        return;
    }

    // create scene layer for surface
    struct wlr_scene_tree *layer_tree =
        shell->get_layer_scene(wlr_layer_surface->pending.layer);
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
        surface->layer_shell->arrange_layers(
            (Output *)surface->wlr_layer_surface->output->data);

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
    };
    wl_signal_add(&wlr_layer_surface->surface->events.unmap, &unmap);

    // commit surface
    commit.notify = [](struct wl_listener *listener, void *data) {
        // on display
        LayerSurface *surface = wl_container_of(listener, surface, commit);

        bool needs_arrange = false;

        // layer changed reparent to new layer tree
        if (surface->wlr_layer_surface->current.committed &
            WLR_LAYER_SURFACE_V1_STATE_LAYER) {
            struct wlr_scene_tree *new_tree =
                surface->layer_shell->get_layer_scene(
                    surface->wlr_layer_surface->current.layer);
            wlr_scene_node_reparent(&surface->scene_layer_surface->tree->node,
                                    new_tree);
            needs_arrange = true;
        }

        wlr_layer_surface_v1 *layer_surface = surface->wlr_layer_surface;

        // get focused output
        Output *output = (Output *)surface->wlr_layer_surface->output->data;

        // get output size
        uint32_t output_width = output->wlr_output->width;
        uint32_t output_height = output->wlr_output->height;

        // set default location to center of screen
        if (!layer_surface->configured) {
            // get usable area of the output
            struct wlr_box usable_area = {0};
            wlr_output_layout_get_box(
                surface->layer_shell->server->output_layout, output->wlr_output,
                &usable_area);

            // Get the actual dimensions of the surface
            uint32_t width = layer_surface->current.actual_width;
            uint32_t height = layer_surface->current.actual_height;

            // calculate target position based on anchor points
            int32_t x = usable_area.x;
            int32_t y = usable_area.y;

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
                width = output_width;
            if (!height)
                height = output_height;

            // send configure with size
            wlr_layer_surface_v1_configure(layer_surface, width, height);
        }

        // rearrange if needed
        if (needs_arrange)
            surface->layer_shell->arrange_layers(
                (Output *)surface->wlr_layer_surface->output->data);
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
    if (wlr_layer_surface->output && wlr_layer_surface->output->data)
        layer_shell->arrange_layers((Output *)wlr_layer_surface->output->data);

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

    if (wlr_layer_surface->current.layer != ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY &&
        wlr_layer_surface->current.keyboard_interactive ==
            ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE)
        return;

    struct wlr_surface *surface = wlr_layer_surface->surface;
    struct wlr_keyboard *keyboard =
        wlr_seat_get_keyboard(layer_shell->server->seat);

    if (keyboard)
        wlr_seat_keyboard_notify_enter(
            layer_shell->server->seat, surface, keyboard->keycodes,
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
