#pragma once

#include "wlr.h"

struct Decoration {
    wl_list link;
    struct Server *server;
    struct Toplevel *toplevel;
    wlr_xdg_toplevel_decoration_v1 *decoration;

    wl_listener mode;
    wl_listener commit;
    wl_listener destroy;

    wlr_xdg_toplevel_decoration_v1_mode client_mode;

    wlr_scene_rect *titlebar;

    Decoration(struct Server *server,
               wlr_xdg_toplevel_decoration_v1 *decoration);
    ~Decoration();

    void create_titlebar(int width);
};
