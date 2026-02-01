#pragma once

#include "wlr.h"

enum class DecorationPart {
    NONE,
    TITLEBAR,
    CLOSE_BUTTON,
    MAXIMIZE_BUTTON,
    FULLSCREEN_BUTTON
};

const int TITLEBAR_HEIGHT = 30;

struct Decoration {
    wl_list link;
    struct Server *server;
    struct Toplevel *toplevel{nullptr};
    wlr_xdg_toplevel_decoration_v1 *decoration;
    struct wlr_scene_tree *scene_tree;

    wl_listener mode;
    wl_listener commit;
    wl_listener destroy;

    wlr_xdg_toplevel_decoration_v1_mode client_mode;

    wlr_scene_rect *titlebar{nullptr};
    wlr_scene_rect *btn_close{nullptr};
    wlr_scene_rect *btn_max{nullptr};
    wlr_scene_rect *btn_full{nullptr};

    bool visible{true};

    Decoration(struct Server *server,
               wlr_xdg_toplevel_decoration_v1 *decoration);
    ~Decoration();

    void create_nodes();
    void set_visible(bool visible);
    void update_titlebar(int width);
    DecorationPart get_part_from_node(struct wlr_scene_node *node);
};
