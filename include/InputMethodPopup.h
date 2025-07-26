#pragma once

#include "wlr.h"

struct InputMethodPopup {
    wl_list link;
    struct InputRelay *relay;
    wlr_input_popup_surface_v2 *popup_surface;
    wlr_scene_tree *scene_tree;

    wl_listener commit;
    wl_listener destroy;

    InputMethodPopup(InputRelay *relay,
                     wlr_input_popup_surface_v2 *popup_surface);
    ~InputMethodPopup();

    void update_position();
};
