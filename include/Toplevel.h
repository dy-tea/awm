#include "wlr.h"

struct Toplevel {
    wl_list link;
    wlr_xdg_toplevel *xdg_toplevel;
    wlr_scene_tree *scene_tree;
    wl_listener map;
    wl_listener unmap;
    wl_listener commit;
    wl_listener destroy;
    wl_listener request_move;
    wl_listener request_resize;
    wl_listener request_maximise;
    wl_listener request_fullscreen;
};
