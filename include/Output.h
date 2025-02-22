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

    Output(struct Server *server, struct wlr_output *wlr_output);
    ~Output();

    void arrange();
    void arrange_layers();

    void update_position();
    bool apply_config(struct OutputConfig *config, bool test_only);

    void arrange_layer_surface(const struct wlr_box *full_area,
                               struct wlr_box *usable_area,
                               struct wlr_scene_tree *tree, bool exclusive);
    struct wlr_scene_tree *shell_layer(enum zwlr_layer_shell_v1_layer layer);

    struct Workspace *new_workspace();
    struct Workspace *get_active();
    struct Workspace *get_workspace(uint32_t n);
    bool set_workspace(uint32_t n);
};
