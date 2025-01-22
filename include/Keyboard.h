#include "wlr.h"

struct Keyboard {
    wl_list link;
    wlr_keyboard keyboard;
    wl_listener modifiers;
    wl_listener key;
    wl_listener destroy;
};
