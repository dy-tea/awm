#pragma once

#include "wlr.h"

struct TextInput {
    struct InputRelay *relay;
    wl_list link;

    wlr_text_input_v3 *wlr_text_input;
    wlr_surface *pending;

    wl_listener enable;
    wl_listener disable;
    wl_listener commit;
    wl_listener destroy;

    TextInput(InputRelay *relay, wlr_text_input_v3 *wlr_text_input);
    ~TextInput();
};
