#include "wlr.h"

struct Keyboard {
    wlr_keyboard keyboard;
    wl_listener modifiers;
    wl_listener key;
    wl_listener destroy;
};
