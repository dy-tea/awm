#pragma once

#include "wlr.h"
#include <stdlib.h>

struct Toplevel;

struct ActivationToken {
    pid_t pid;

    wlr_xdg_activation_token_v1 *token;
    wl_listener destroy;

    bool activated, had_focus;

    wl_list link;

    Toplevel *owning_toplevel{nullptr};

    ActivationToken(struct Server *server, wlr_xdg_activation_token_v1 *token);
    ~ActivationToken();

    void consume();
};
