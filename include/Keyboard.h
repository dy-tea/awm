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

    void update_config();
    bool handle_bind(struct Bind bind, uint32_t keycode);
    uint32_t keysyms_raw(xkb_keycode_t keycode, const xkb_keysym_t **keysyms,
                         uint32_t *modifiers);
    uint32_t keysyms_translated(xkb_keycode_t keycode,
                                const xkb_keysym_t **keysyms,
                                uint32_t *modifiers);
};
