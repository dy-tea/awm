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
    wl_listener new_xdg_popup;
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

    wlr_foreign_toplevel_handle_v1 *foreign_handle;
    wl_listener foreign_activate;
    wl_listener foreign_close;

    wlr_ext_foreign_toplevel_handle_v1 *ext_foreign_handle;
    wl_listener ext_foreign_destroy;

    bool hidden{false};

    wlr_box geometry{};
    wlr_box saved_geometry{};

    Toplevel(Server *server, wlr_xdg_toplevel *wlr_xdg_toplevel);
    ~Toplevel();

#ifdef XWAYLAND
    Toplevel(Server *server, wlr_xwayland_surface *xwayland_surface);
#endif

    static void map_notify(wl_listener *listener, void *data);
    static void unmap_notify(wl_listener *listener, void *data);

    void create_foreign();

    void create_ext_foreign();
    void update_ext_foreign() const;

    std::string title() const;
    std::string app_id() const;
    void update_title();
    void update_app_id();

    void focus() const;
    void begin_interactive(CursorMode mode, uint32_t edges);
    void set_position_size(double x, double y, int width, int height);
    void set_position_size(const wlr_box &geometry);
    wlr_box get_geometry();
    void set_hidden(bool hidden);
    bool fullscreen() const;
    bool maximized() const;
    void set_fullscreen(bool fullscreen);
    void set_maximized(bool maximized);
    void toggle_fullscreen();
    void toggle_maximized();
    void save_geometry();
    void close() const;
};
