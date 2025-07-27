#pragma once

#include "wlr.h"

struct Keyboard {
    wl_list link;
    struct Server *server;
    struct wlr_keyboard *wlr_keyboard;

    wl_listener modifiers;
    wl_listener key;
    wl_listener destroy;

    Keyboard(Server *server, struct wlr_keyboard *keyboard);
    ~Keyboard();

    void update_config() const;
    uint32_t keysyms_raw(xkb_keycode_t keycode,
                         const xkb_keysym_t **keysyms) const;
    uint32_t keysyms_translated(xkb_keycode_t keycode,
                                const xkb_keysym_t **keysyms) const;
};
