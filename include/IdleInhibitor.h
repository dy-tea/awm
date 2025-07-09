#pragma once

#include "Server.h"
#include "wlr.h"

struct IdleInhibitor {
    wlr_idle_inhibitor_v1 *idle_inhibitor;
    Server *server;

    wl_listener destroy;

    IdleInhibitor(wlr_idle_inhibitor_v1 *idle_inhibitor, Server *server);
    ~IdleInhibitor();
};
