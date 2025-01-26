#include "Server.h"
#include "wlr.h"

LayerSurface::LayerSurface(struct wlr_layer_surface_v1* wlr_layer_surface) {
    this->wlr_layer_surface = wlr_layer_surface;
    mapped = false;
    wlr_layer_surface->data = this;
    wlr_layer_surface->initialized = true;

    // map surface
    map.notify = [](struct wl_listener *listener, void *data) {
        LayerSurface *surface = wl_container_of(listener, surface, map);
        surface->wlr_layer_surface->surface = (wlr_surface*)data;
        surface->mapped = true;
        wlr_log(WLR_DEBUG, "Mapped wlr_layer_surface_v1");
    };
    wl_signal_add(&wlr_layer_surface->surface->events.map, &map);

    // unmap surface
    unmap.notify = [](struct wl_listener *listener, void *data) {
        LayerSurface *surface = wl_container_of(listener, surface, unmap);
        surface->wlr_layer_surface->surface = nullptr;
        surface->mapped = false;
        wlr_log(WLR_DEBUG, "Unmapped wlr_layer_surface_v1");
    };
    wl_signal_add(&wlr_layer_surface->surface->events.unmap, &unmap);

    // commit surface
    commit.notify = [](struct wl_listener *listener, void *data) {
        LayerSurface *surface = wl_container_of(listener, surface, commit);

        if (!surface->initialized)
            return;

        wlr_layer_surface_v1 *layer_surface = surface->wlr_layer_surface;
        wlr_output *output = layer_surface->output;

        if (output == nullptr)
            return;

        uint32_t dw = layer_surface->current.desired_width;
        uint32_t dh = layer_surface->current.desired_height;

        if (!dw) dw = output->width;
        if (!dh) dh = output->height;

        surface->serial = wlr_layer_surface_v1_configure(layer_surface, dw, dh);
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

    wlr_surface_map(wlr_layer_surface->surface);
}

LayerSurface::~LayerSurface() {
    wlr_layer_surface_v1_destroy(wlr_layer_surface);
}
