#include "wlr.h"

struct Output {
    struct wl_list link;
    struct Server *server;
    struct wlr_output *wlr_output;
    struct wl_listener frame;
    struct wl_listener request_state;
    struct wl_listener destroy;

    struct wl_list workspaces;
    uint32_t max_workspace{0};

    Output(struct Server *server, struct wlr_output *wlr_output);
    ~Output();

    struct wlr_box get_usable_area();
    struct Workspace *new_workspace();
    struct Workspace *get_active();
    struct Workspace *get_workspace(uint32_t n);
    bool set_workspace(uint32_t n);
};
