#include "wlr.h"

struct LayerSurface {
    struct wl_list link;
    struct LayerShell *layer_shell;
    struct wlr_layer_surface_v1 *wlr_layer_surface;
    struct wlr_scene_layer_surface_v1 *scene_layer_surface;
    struct wl_list popups;
    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener commit;
    struct wl_listener new_popup;
    struct wl_listener destroy;

    LayerSurface(struct LayerShell *shell, struct wlr_layer_surface_v1* wlr_layer_surface);
    ~LayerSurface();

    void handle_focus();
};
