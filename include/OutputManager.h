#include "wlr.h"

struct OutputManager {
    struct Server *server;

    struct wl_list outputs;
    struct wlr_output_layout *layout;

    struct wlr_xdg_output_manager_v1 *wlr_xdg_output_manager;
    struct wlr_output_manager_v1 *wlr_output_manager;
    struct wl_listener apply;
    struct wl_listener test;
    struct wl_listener destroy;
    struct wl_listener new_output;
    struct wl_listener change;

    OutputManager(struct Server *server);
    ~OutputManager();

    bool apply_to_output(struct Output *output, struct OutputConfig *config,
                         bool test_only);
    void apply_config(struct wlr_output_configuration_v1 *cfg, bool test_only);

    struct Output *get_output(struct wlr_output *wlr_output);
    struct Output *output_at(double x, double y);

    void arrange();
};
