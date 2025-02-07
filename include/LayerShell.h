#include "LayerSurface.h"

struct LayerShell {
    struct wl_list layer_surfaces;
    struct Server *server;
    struct wlr_layer_shell_v1 *wlr_layer_shell;
    struct wl_listener new_shell_surface;
    struct wl_listener destroy;

    LayerShell(struct Server *server);
    ~LayerShell();
};
