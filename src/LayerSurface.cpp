#include "Server.h"
#include "wlr-layer-shell-unstable-v1-protocol.h"
#include "wlr.h"
#include "wlr/util/log.h"
#include <wayland-util.h>

LayerSurface::LayerSurface(struct LayerShell *shell, struct wlr_layer_surface_v1* wlr_layer_surface) {
    this->wlr_layer_surface = wlr_layer_surface;
    wl_list_insert(&shell->layer_surfaces, &link);

    scene_layer_surface = wlr_scene_layer_surface_v1_create(&shell->scene->tree, wlr_layer_surface);

    if (!scene_layer_surface) {
        wlr_log(WLR_ERROR, "failed to create scene layer surface");
        return;
    }

    wlr_layer_surface->data = this;

    // map surface
    map.notify = [](struct wl_listener *listener, void *data) {
        LayerSurface *surface = wl_container_of(listener, surface, map);
        wlr_scene_node_set_enabled(&surface->scene_layer_surface->tree->node, true);
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

        wlr_xdg_popup *xdg_popup = (wlr_xdg_popup*)data;

        struct Popup *popup = new Popup(xdg_popup);
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
    wlr_layer_surface_v1_destroy(wlr_layer_surface);
}
