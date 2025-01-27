#include "LayerSurface.h"

struct LayerShell {
    struct wl_list layer_surfaces;
    struct wlr_layer_shell_v1 *wlr_layer_shell;
    struct wlr_scene *scene;
    struct wlr_seat *seat;
    struct wl_listener new_shell_surface;
    struct wl_listener destroy;

    LayerShell(struct wl_display *wl_display, struct wlr_scene *scene, struct wlr_seat *seat);
    ~LayerShell();
};
