#pragma once

#include "wlr.h"

struct TearingController {
    wl_list link;
    struct Server *server;
    wlr_tearing_control_v1 *tearing_control;

    wl_listener set_hint;
    wl_listener destroy;

    TearingController(Server *server, wlr_tearing_control_v1 *tearing_control);
    ~TearingController();
};
