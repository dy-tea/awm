#include "Cursor.h"

struct Toplevel {
    wl_list link;
    Server *server;
    wlr_scene_tree *scene_tree{nullptr};
    wlr_scene_surface *scene_surface{nullptr};

    wlr_xdg_toplevel *xdg_toplevel{nullptr};

    wl_listener map;
    wl_listener unmap;
    wl_listener commit;
    wl_listener destroy;
    wl_listener request_move;
    wl_listener request_resize;
    wl_listener request_maximize;
    wl_listener request_fullscreen;
    wl_listener request_minimize;
    //  wl_listener request_show_window_menu;
    //  wl_listener set_parent;
    //  wl_listener set_title;
    //  wl_listener set_app_id;

#ifdef XWAYLAND
    wlr_xwayland_surface *xwayland_surface{nullptr};

    wl_listener activate;
    wl_listener associate;
    wl_listener dissociate;
    wl_listener configure;
    wl_listener xwayland_commit;
    wl_listener xwayland_resize;
    wl_listener xwayland_move;
    wl_listener xwayland_maximize;
    wl_listener xwayland_fullscreen;
    wl_listener xwayland_close;
#endif

    wlr_foreign_toplevel_handle_v1 *handle{nullptr};

    wl_listener handle_request_maximize;
    wl_listener handle_request_minimize;
    wl_listener handle_request_fullscreen;
    wl_listener handle_request_activate;
    wl_listener handle_request_close;
    wl_listener handle_set_rectangle;
    wl_listener handle_destroy;

    bool hidden{false};

    wlr_fbox saved_geometry;

    Toplevel(Server *server, wlr_xdg_toplevel *wlr_xdg_toplevel);
    ~Toplevel();

#ifdef XWAYLAND
    Toplevel(Server *server, wlr_xwayland_surface *xwayland_surface);
#endif

    static void map_notify(wl_listener *listener, void *data);
    static void unmap_notify(wl_listener *listener, void *data);

    void create_handle();

    void focus() const;
    void begin_interactive(CursorMode mode, uint32_t edges);
    void set_position_size(double x, double y, int width, int height);
    void set_position_size(const wlr_fbox &geometry);
    wlr_fbox get_geometry() const;
    void set_hidden(bool hidden);
    void set_fullscreen(bool fullscreen);
    void set_maximized(bool maximized);
    void save_geometry();
    void close() const;

    void update_foreign_toplevel() const;
};
