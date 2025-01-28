#include "Server.h"

LayerSurface::LayerSurface(struct LayerShell *shell, struct wlr_layer_surface_v1* wlr_layer_surface) {
    layer_shell = shell;
    this->wlr_layer_surface = wlr_layer_surface;
    wl_list_insert(&shell->layer_surfaces, &link);
    wl_list_init(&popups);

    scene_layer_surface = wlr_scene_layer_surface_v1_create(&shell->scene->tree, wlr_layer_surface);

    if (!scene_layer_surface) {
        wlr_log(WLR_ERROR, "failed to create scene layer surface");
        return;
    }

    wlr_layer_surface->data = this;

    wlr_layer_surface->current.keyboard_interactive = ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND;
    wlr_layer_surface->current.layer = ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY;

    // map surface
    map.notify = [](struct wl_listener *listener, void *data) {
        LayerSurface *surface = wl_container_of(listener, surface, map);
        wlr_scene_node_set_enabled(&surface->scene_layer_surface->tree->node, true);

        if (surface->wlr_layer_surface->current.keyboard_interactive)
            surface->handle_focus();
    };
    wl_signal_add(&wlr_layer_surface->surface->events.map, &map);

    // unmap surface
    unmap.notify = [](struct wl_listener *listener, void *data) {
        LayerSurface *surface = wl_container_of(listener, surface, unmap);
        wlr_scene_node_set_enabled(&surface->scene_layer_surface->tree->node, false);
        wlr_log(WLR_DEBUG, "Unmapped wlr_layer_surface_v1");
    };
    wl_signal_add(&wlr_layer_surface->surface->events.unmap, &unmap);

    // commit surface
    commit.notify = [](struct wl_listener *listener, void *data) {
        LayerSurface *surface = wl_container_of(listener, surface, commit);

        wlr_layer_surface_v1 *layer_surface = surface->wlr_layer_surface;

        if (!layer_surface->configured) {
            wlr_output *output = layer_surface->output;

            if (output == nullptr) {
               wlr_log(WLR_ERROR, "Layer surface has no output");
               return;
            }

            uint32_t dw = layer_surface->current.desired_width;
            uint32_t dh = layer_surface->current.desired_height;

            if (!dw) dw = output->width;
            if (!dh) dh = output->height;

            wlr_layer_surface_v1_configure(layer_surface, dw, dh);
            return;
        }
    };
    wl_signal_add(&wlr_layer_surface->surface->events.commit, &commit);

    // new_popup
    new_popup.notify = [](struct wl_listener *listener, void *data) {
        LayerSurface *layer_surface = wl_container_of(listener, layer_surface, wlr_layer_surface);

        struct Popup *popup = new Popup((wlr_xdg_popup*)data);
        wl_list_insert(&layer_surface->popups, &popup->link);
    };
    wl_signal_add(&wlr_layer_surface->events.new_popup, &new_popup);

    // destroy
    destroy.notify = [](struct wl_listener *listener, void *data) {
        wlr_log(WLR_DEBUG, "Destroy called");
        LayerSurface *surface = wl_container_of(listener, surface, destroy);
        delete surface;
    };
    wl_signal_add(&wlr_layer_surface->events.destroy, &destroy);
}

LayerSurface::~LayerSurface() {
    wl_list_remove(&map.link);
    wl_list_remove(&unmap.link);
    wl_list_remove(&commit.link);
    wl_list_remove(&new_popup.link);
    wl_list_remove(&destroy.link);
}

void LayerSurface::handle_focus() {
    if (!wlr_layer_surface->surface->mapped)
        return;

    struct wlr_surface *surface = wlr_layer_surface->surface;
    struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(layer_shell->seat);

    wlr_seat_keyboard_notify_enter(layer_shell->seat, surface, keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);;
}
