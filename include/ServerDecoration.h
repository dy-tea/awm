#pragma once

#include "Server.h"

struct ServerDecoration {
    wl_list link;
    Server *server;
    wlr_server_decoration *decoration;
    wl_listener mode;
    wl_listener destroy;

    ServerDecoration(Server *server, wlr_server_decoration *decoration);
    ~ServerDecoration();
};
