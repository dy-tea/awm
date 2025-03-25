#pragma once

#include "wlr.h"

struct LayerSurface {
    wl_list link;
    struct Output *output;
    wlr_scene_tree *scene_tree;
    wlr_layer_surface_v1 *wlr_layer_surface;
    wlr_scene_layer_surface_v1 *scene_layer_surface;
    wl_listener map;
    wl_listener unmap;
    wl_listener commit;
    wl_listener new_popup;
    wl_listener destroy;

    bool mapped{false};

    LayerSurface(Output *output, wlr_layer_surface_v1 *wlr_layer_surface);
    ~LayerSurface();

    void handle_focus() const;
    bool should_focus() const;
};
