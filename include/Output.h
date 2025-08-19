#pragma once

#include "Workspace.h"
#include "wlr.h"

struct Output {
    wl_list link;
    struct Server *server;
    struct wlr_output *wlr_output;
    wlr_scene_output *scene_output;

    wl_listener frame;
    wl_listener present;
    wl_listener request_state;
    wl_listener destroy;

    wlr_box usable_area;
    wlr_box layout_geometry;

    struct {
        wlr_scene_tree *background;
        wlr_scene_tree *bottom;
        wlr_scene_tree *top;
        wlr_scene_tree *overlay;
    } layers;

    wlr_session_lock_surface_v1 *lock_surface{nullptr};
    wl_listener destroy_lock_surface;

    wl_event_source *repaint_timer;

    timespec last_present{};
    int refresh_nsec{};

    uint32_t max_render_time{0};
    bool allow_tearing{false};
    bool enabled{true};

    Output(Server *server, struct wlr_output *wlr_output);
    ~Output();

    void arrange();
    void arrange_layers();

    void update_position();
    bool apply_config(struct OutputConfig *config, bool test_only);

    static void arrange_layer_surface(const wlr_box *full_area,
                                      wlr_box *usable_area,
                                      const wlr_scene_tree *tree,
                                      bool exclusive);
    wlr_scene_tree *shell_layer(enum zwlr_layer_shell_v1_layer layer) const;

    Workspace *new_workspace();
    Workspace *get_active();
    Workspace *get_workspace(uint32_t n);
    bool set_workspace(Workspace *workspace);
    bool set_workspace(uint32_t n);
};
