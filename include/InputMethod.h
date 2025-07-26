#pragma once

#include "wlr.h"

struct InputMethod {
    struct InputRelay *relay;
    wlr_input_method_v2 *wlr_input_method;

    wl_listener commit;
    wl_listener grab_keyboard;
    wl_listener new_popup_surface;
    wl_listener destroy;

    InputMethod(InputRelay *relay, wlr_input_method_v2 *wlr_input_method);
    ~InputMethod();

    bool is_keyboard_emulated_by_input_method(wlr_keyboard *keyboard) const;
};
