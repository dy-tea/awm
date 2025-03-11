#include "Config.h"

struct Keyboard {
    wl_list link;
    Server *server;
    struct wlr_keyboard *wlr_keyboard;
    wlr_virtual_keyboard_v1 *virtual_keyboard{nullptr};

    wl_listener modifiers;
    wl_listener key;
    wl_listener destroy;

    Keyboard(Server *server, struct wlr_keyboard *keyboard);
    Keyboard(Server *server, wlr_virtual_keyboard_v1 *virtual_keyboard);
    ~Keyboard();

    void update_config() const;
    bool handle_bind(Bind bind);
    uint32_t keysyms_raw(xkb_keycode_t keycode, const xkb_keysym_t **keysyms,
                         uint32_t *modifiers) const;
    uint32_t keysyms_translated(xkb_keycode_t keycode,
                                const xkb_keysym_t **keysyms,
                                uint32_t *modifiers) const;
};
