#pragma once

#include <atomic>
#include <cassert>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

#include "IPC.h"
#include "Keyboard.h"
#include "LayerSurface.h"
#include "Output.h"
#include "OutputManager.h"
#include "PointerConstraint.h"
#include "Popup.h"
#include "ServerDecoration.h"
#include "SessionLock.h"
#include "TextInput.h"
#include "Toplevel.h"
#include "Workspace.h"
#include "wlr.h"

struct Server {
    // singleton
    static Server *instance;
    static Server *get(Config *config) {
        if (!instance)
            instance = new Server(config);
        return instance;
    }
    static Server *get() { return instance; }
    Server() = default;
    Server(const Server &other) = delete;

    Config *config;

    wl_display *display;
    wlr_session *session;
    wlr_backend *backend;
    wlr_renderer *renderer;
    wlr_allocator *allocator;
    wlr_compositor *compositor;
    wlr_scene *scene;
    wlr_scene_output_layout *scene_layout;

    wlr_linux_dmabuf_v1 *wlr_linux_dmabuf{nullptr};

    wl_listener renderer_lost;

    int drm_fd;

    wlr_xdg_shell *xdg_shell;
    wl_listener new_xdg_toplevel;

    wlr_relative_pointer_manager_v1 *wlr_relative_pointer_manager;

    Cursor *cursor;

    wlr_seat *seat;
    wl_listener new_input;
    wl_listener request_cursor;
    wl_listener request_set_selection;
    wl_listener request_set_primary_selection;
    wl_listener request_start_drag;
    wl_listener start_drag;
    wl_listener destroy_drag_icon;

    wl_list keyboards;

    Toplevel *grabbed_toplevel;

    OutputManager *output_manager;

    struct {
        wlr_scene_tree *background;
        wlr_scene_tree *bottom;
        wlr_scene_tree *floating;
        wlr_scene_tree *fullscreen;
        wlr_scene_tree *top;
        wlr_scene_tree *overlay;
        wlr_scene_tree *drag_icon;
        wlr_scene_tree *lock;
    } layers;

    wl_list layer_surfaces;
    wlr_layer_shell_v1 *wlr_layer_shell;
    wl_listener new_shell_surface;

    wlr_session_lock_manager_v1 *wlr_session_lock_manager;
    wlr_session_lock_v1 *current_session_lock{nullptr};
    wl_listener new_session_lock;
    wlr_scene_rect *lock_background;
    bool locked{false};

    wlr_virtual_pointer_manager_v1 *virtual_pointer_mgr;
    wl_listener new_virtual_pointer;

    wlr_virtual_keyboard_manager_v1 *virtual_keyboard_manager;
    wl_listener new_virtual_keyboard;

    wlr_pointer_constraints_v1 *wlr_pointer_constraints;
    wl_listener new_pointer_constraint;

    wlr_xdg_activation_v1 *wlr_xdg_activation;
    wl_listener xdg_activation_activate;

    wlr_text_input_manager_v3 *wlr_text_input_manager;
    wl_listener new_text_input;

    wlr_xdg_system_bell_v1 *wlr_xdg_system_bell;
    wl_listener ring_system_bell;

#ifdef SERVER_DECORATION
    struct wlr_server_decoration_manager *wlr_server_decoration_manager;
    wl_listener new_server_decoration;

    wlr_xdg_decoration_manager_v1 *xdg_decoration_manager;
    wl_listener new_decoration;
    wl_list decorations;
#endif

    wlr_input_method_manager_v2 *wlr_input_method_manager;
    wlr_ext_foreign_toplevel_list_v1 *wlr_foreign_toplevel_list;
    wlr_foreign_toplevel_manager_v1 *wlr_foreign_toplevel_manager;
    wlr_idle_notifier_v1 *wlr_idle_notifier;

#ifdef XWAYLAND
    wlr_xwayland *xwayland;
    wl_listener xwayland_ready;
    wl_listener new_xwayland_surface;
#endif

    struct sigaction sa{};
    std::thread config_thread;
    std::atomic<bool> running{true};

    IPC *ipc{nullptr};

    Server(Config *config);
    ~Server();

    void exit() const;

    void spawn(const std::string &command) const;

    Output *get_output(const wlr_output *wlr_output) const;
    Workspace *get_workspace(Toplevel *toplevel) const;
    Toplevel *get_toplevel(wlr_surface *surface) const;
#ifdef SERVER_DECORATION
    ServerDecoration *get_server_decoration(wlr_surface *surface) const;
#endif

    Output *focused_output() const;

    template <typename T>
    T *surface_at(double lx, double ly, wlr_surface **surface, double *sx,
                  double *sy);
    Toplevel *toplevel_at(double lx, double ly, wlr_surface **surface,
                          double *sx, double *sy);
    LayerSurface *layer_surface_at(double lx, double ly, wlr_surface **surface,
                                   double *sx, double *sy);

    bool handle_bind(Bind bind);
};
