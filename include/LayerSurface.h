#include "wlr.h"

struct LayerSurface {
    struct wlr_layer_surface_v1* wlr_layer_surface;
    struct wl_listener new_popup;
    struct wl_listener destroy;

    LayerSurface(struct wlr_surface* wlr_surface);
    ~LayerSurface();
};
