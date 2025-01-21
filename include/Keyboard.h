#include "wlr.h"
#include <vector>

struct Keyboard {
    std::vector<Keyboard*> *link;
    wlr_keyboard keyboard;
    wl_listener modifiers;
    wl_listener key;
    wl_listener destroy;
};
