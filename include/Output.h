#include "wlr.h"

struct Output {
    struct wl_list link;
    struct Server *server;
    struct wlr_output *wlr_output;
    struct wl_listener frame;
    struct wl_listener request_state;
    struct wl_listener destroy;

    struct wlr_box usable_area;

    struct {
        struct wlr_scene_tree *background;
        struct wlr_scene_tree *bottom;
        struct wlr_scene_tree *top;
        struct wlr_scene_tree *overlay;
    } layers;

    wlr_scene_output *scene_output;

    wlr_box layout_geometry;

    struct wl_list workspaces;
    uint32_t max_workspace{0};

    wlr_session_lock_surface_v1 *lock_surface{nullptr};
    wl_listener destroy_lock_surface;

    Output(struct Server *server, struct wlr_output *wlr_output);
    ~Output();

    void arrange();
    void arrange_layers();

    void update_position();
    bool apply_config(const OutputConfig *config, bool test_only);

    static void arrange_layer_surface(const wlr_box *full_area,
                                      wlr_box *usable_area,
                                      const wlr_scene_tree *tree,
                                      bool exclusive);
    struct wlr_scene_tree *
    shell_layer(enum zwlr_layer_shell_v1_layer layer) const;

    struct Workspace *new_workspace();
    struct Workspace *get_active() const;
    struct Workspace *get_workspace(uint32_t n) const;
    bool set_workspace(uint32_t n);
};
