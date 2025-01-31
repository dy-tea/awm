#include "wlr.h"

struct Output {
    struct wl_list link;
    struct Server *server;
    struct wlr_output *wlr_output;
    struct wl_listener frame;
    struct wl_listener request_state;
    struct wl_listener destroy;

    struct wl_list workspaces;

    Output(struct Server *server, struct wlr_output *wlr_output);
    ~Output();

    struct Workspace *new_workspace();
    struct Workspace *get_workspace(uint32_t n);
};
