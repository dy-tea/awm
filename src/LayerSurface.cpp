#include "Server.h"

LayerSurface::LayerSurface(struct LayerShell *shell,
                           struct wlr_layer_surface_v1 *wlr_layer_surface) {
    layer_shell = shell;
    // create scene layer for surface
    this->wlr_layer_surface = wlr_layer_surface;
    wl_list_init(&popups);
    scene_layer_surface = wlr_scene_layer_surface_v1_create(
        &shell->server->scene->tree, wlr_layer_surface);

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

        if (surface->wlr_layer_surface->current.keyboard_interactive)
            surface->handle_focus();
    };
    wl_signal_add(&wlr_layer_surface->surface->events.map, &map);

    // unmap surface
    unmap.notify = [](struct wl_listener *listener, void *data) {
        LayerSurface *surface = wl_container_of(listener, surface, unmap);
        wlr_scene_node_set_enabled(&surface->scene_layer_surface->tree->node,
                                   false);
    };
    wl_signal_add(&wlr_layer_surface->surface->events.unmap, &unmap);

    // commit surface
    commit.notify = [](struct wl_listener *listener, void *data) {
        // on display
        LayerSurface *surface = wl_container_of(listener, surface, commit);

        wlr_layer_surface_v1 *layer_surface = surface->wlr_layer_surface;

        // configure if not
        if (!layer_surface->configured) {
            // get focused output
            Output *output = surface->layer_shell->server->focused_output();

            // get output size
            uint32_t output_width = output->wlr_output->width;
            uint32_t output_height = output->wlr_output->height;

            // get output layout position
            struct wlr_box box;
            wlr_output_layout_get_box(
                surface->layer_shell->server->output_layout, output->wlr_output,
                &box);

            // get desired size
            uint32_t width = layer_surface->current.desired_width;
            uint32_t height = layer_surface->current.desired_height;

            // if client's desired size is too small or too big give it the full
            // size for the client to decide
            if (width <= 100 || width > output_width)
                width = output_width;
            if (height <= 100 || height > output_height)
                height = output_height;

            // calculate target position
            int32_t x = output_width / 2 - width / 2 + box.x;
            int32_t y = output_height / 2 - height / 2 + box.y;

            // ensure the target position is on-screen
            x = std::max(0, x);
            y = std::max(0, y);

            // set the postion to the center of the screen
            wlr_scene_node_set_position(
                &surface->scene_layer_surface->tree->node, x, y);

            // send configure with size
            wlr_layer_surface_v1_configure(layer_surface, width, height);

            return;
        }
    };
    wl_signal_add(&wlr_layer_surface->surface->events.commit, &commit);

    // new_popup
    new_popup.notify = [](struct wl_listener *listener, void *data) {
        LayerSurface *layer_surface =
            wl_container_of(listener, layer_surface, new_popup);

        // add to list of popups
        struct Popup *popup = new Popup((wlr_xdg_popup *)data);
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
    if (scene_layer_surface)
        wlr_scene_node_destroy(&scene_layer_surface->tree->node);

    struct Popup *popup, *tmp;
    wl_list_for_each_safe(popup, tmp, &popups, link) delete popup;

    wl_list_remove(&link);
    wl_list_remove(&map.link);
    wl_list_remove(&unmap.link);
    wl_list_remove(&commit.link);
    wl_list_remove(&new_popup.link);
    wl_list_remove(&destroy.link);
}

void LayerSurface::handle_focus() {
    if (!wlr_layer_surface || !wlr_layer_surface->surface || !wlr_layer_surface->surface->mapped || !scene_layer_surface)
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

bool LayerSurface::should_focus() {
    if (!wlr_layer_surface)
        return false;

    return wlr_layer_surface->current.keyboard_interactive !=
           ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE;
}
