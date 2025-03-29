#pragma once

#include "wlr.h"

struct TextInput {
    Server *server;
    wlr_text_input_v3 *wlr_text_input;
    wlr_input_method_v2 *input_method;
    wlr_surface *pending;

    wl_listener enable;
    wl_listener commit;
    wl_listener disable;
    wl_listener destroy;

    TextInput(Server *server, wlr_text_input_v3 *wlr_text_input);
    ~TextInput();
};
