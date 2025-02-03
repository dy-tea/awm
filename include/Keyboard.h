#include "Config.h"

struct Keyboard {
    struct wl_list link;
    struct Server *server;
    struct wlr_keyboard *wlr_keyboard;

    struct wl_listener modifiers;
    struct wl_listener key;
    struct wl_listener destroy;

    Keyboard(struct Server *server, struct wlr_input_device *device);
    ~Keyboard();

    bool handle_bind(struct Bind bind, uint32_t keycode);
};
