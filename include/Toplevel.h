#include "Cursor.h"

struct Toplevel {
    struct wl_list link;
    struct Server *server;
    struct wlr_xdg_toplevel *xdg_toplevel;
    struct wlr_scene_tree *scene_tree;

    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener commit;
    struct wl_listener destroy;
    struct wl_listener request_move;
    struct wl_listener request_resize;
    struct wl_listener request_maximize;
    struct wl_listener request_fullscreen;
    // struct wl_listener request_minimize;
    // struct wl_listener request_show_window_menu;
    // struct wl_listener set_parent;
    // struct wl_listener set_title;
    // struct wl_listener set_app_id;

    struct wlr_foreign_toplevel_handle_v1 *handle;

    struct wl_listener handle_request_maximize;
    struct wl_listener handle_request_fullscreen;

    bool hidden{false};

    struct wlr_fbox saved_geometry;

    Toplevel(struct Server *server, struct wlr_xdg_toplevel *wlr_xdg_toplevel);
    ~Toplevel();

    void focus();
    void begin_interactive(enum CursorMode mode, uint32_t edges);
    void set_position_size(double x, double y, int width, int height);
    void set_position_size(struct wlr_fbox geometry);
    struct wlr_fbox get_geometry();
    void set_hidden(bool hidden);
    void set_fullscreen(bool fullscreen);
    void set_maximized(bool maximized);
    void close();

    void update_foreign_toplevel();
};
