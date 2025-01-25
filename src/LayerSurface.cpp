#include "Server.h"
#include "wlr.h"

LayerSurface::LayerSurface(struct wlr_surface *wlr_surface) {
    wlr_layer_surface = wlr_layer_surface_v1_try_from_wlr_surface(wlr_surface);
    if (wlr_layer_surface == NULL) {
       wlr_log(WLR_ERROR, "Failed to create wlr_layer_surface_v1 from wlr_surface");
       return;
    }

    uint32_t width = wlr_layer_surface->pending.desired_width;
    uint32_t height = wlr_layer_surface->pending.desired_height;

    wlr_layer_surface_v1_configure(wlr_layer_surface, width, height);

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

    // output may be null
    if (wlr_layer_surface->output == nullptr) {
        Server *server = wl_container_of(wlr_surface, server, outputs);
        Output *output = wl_container_of(server, output, link);
        wlr_layer_surface->output = output->wlr_output;
    }
}

LayerSurface::~LayerSurface() {
    wlr_layer_surface_v1_destroy(wlr_layer_surface);
}
