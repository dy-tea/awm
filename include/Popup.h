#pragma once

#include "wlr.h"

struct Popup {
    Server *server;
    wl_list link;
    wlr_xdg_popup *xdg_popup;
    wlr_scene_tree *parent_tree;

    wl_listener new_popup;
    wl_listener commit;
    wl_listener destroy;

    Popup(wlr_xdg_popup *xdg_popup, wlr_scene_tree *parent_tree,
          Server *server);
    ~Popup();
};
