#pragma once

#include "Cursor.h"
#include <string>

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
    bool xwayland_maximized{false};

    wl_listener activate;
    wl_listener associate;
    wl_listener dissociate;
    wl_listener configure;
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

    wlr_xdg_dialog_v1 *wlr_xdg_dialog{nullptr};
    wl_listener xdg_dialog_destroy;

    wlr_xdg_toplevel_decoration_v1 *xdg_decoration{nullptr};
    wl_listener set_decoration_mode;
    wl_listener destroy_decoration;

    bool hidden{false};
    wlr_server_decoration_manager_mode ssd_mode{
        WLR_SERVER_DECORATION_MANAGER_MODE_CLIENT};
    bool using_csd{true};

    wlr_box geometry{};
    wlr_box saved_geometry{};

    Toplevel(Server *server, wlr_xdg_toplevel *wlr_xdg_toplevel);
    ~Toplevel();

#ifdef XWAYLAND
    Toplevel(Server *server, wlr_xwayland_surface *xwayland_surface);
#endif

    static void map_notify(wl_listener *listener, void *data);
    static void unmap_notify(wl_listener *listener, void *data);
    static void request_decoration_mode(wl_listener *listener, void *data);

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
    void set_ssd_mode(wlr_server_decoration_manager_mode mode);
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
