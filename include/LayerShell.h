#include "LayerSurface.h"

struct LayerShell {
    struct wlr_scene_tree *layers[4];
    struct wl_list layer_surfaces;
    struct Server *server;
    struct wlr_layer_shell_v1 *wlr_layer_shell;
    struct wl_listener new_shell_surface;
    struct wl_listener destroy;

    LayerShell(struct Server *server);
    ~LayerShell();

    void arrange_layers(struct Output *output);
    struct wlr_scene_tree *get_layer_scene(enum zwlr_layer_shell_v1_layer type);
};
