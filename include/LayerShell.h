#include <vector>
#include <memory>
#include "wlr.h"
#include "LayerSurface.h"

struct LayerShell {
    wlr_layer_shell_v1 *wlr_layer_shell;
    struct wl_listener new_shell_surface;
    struct wl_listener destroy;
    std::vector<std::unique_ptr<LayerSurface>> pending;

    LayerShell(struct wl_display* wl_display);
    ~LayerShell();
};
