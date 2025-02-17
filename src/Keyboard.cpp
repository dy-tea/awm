#include "Server.h"

// execute either a wm bind or command bind, returns true if
// bind is valid, false otherwise
bool Keyboard::handle_bind(struct Bind bind, uint32_t keycode) {
    // retrieve config
    Config *config = server->config;

    // get current output
    Output *output = server->focused_output();
    if (output == NULL)
        return false;

    // handle user-defined binds
    for (std::pair<Bind, std::string> command : config->commands)
        if (command.first == bind)
            if (fork() == 0) {
                execl("/bin/sh", "/bin/sh", "-c", command.second.c_str(),
                      nullptr);
                return true;
            }

    // handle compositor binds
    if (bind == config->exit) {
        // exit compositor
        wl_display_terminate(server->wl_display);
    } else if (bind == config->window_fullscreen) {
        // fullscreen the active toplevel
        Toplevel *active = output->get_active()->active_toplevel;

        if (active == nullptr)
            return false;

        active->set_fullscreen(!active->xdg_toplevel->current.fullscreen);
    } else if (bind == config->window_previous) {
        // focus the previous toplevel in the active workspace
        output->get_active()->focus_prev();
    } else if (bind == config->window_next) {
        // focus the next toplevel in the active workspace
        output->get_active()->focus_next();
    } else if (bind == config->window_move) {
        // move the active toplevel with the mouse
        Toplevel *active = output->get_active()->active_toplevel;
        if (active)
            active->begin_interactive(CURSORMODE_MOVE, 0);
    } else if (bind == config->window_up) {
        // focus the toplevel in the up direction
        Toplevel *up = output->get_active()->in_direction(WLR_DIRECTION_UP);
        if (up)
            output->get_active()->focus_toplevel(up);
    } else if (bind == config->window_down) {
        // focus the toplevel in the down direction
        Toplevel *down = output->get_active()->in_direction(WLR_DIRECTION_DOWN);
        if (down)
            output->get_active()->focus_toplevel(down);
    } else if (bind == config->window_left) {
        // focus the toplevel in the left direction
        Toplevel *left = output->get_active()->in_direction(WLR_DIRECTION_LEFT);
        if (left)
            output->get_active()->focus_toplevel(left);
    } else if (bind == config->window_right) {
        // focus the toplevel in the right direction
        Toplevel *right =
            output->get_active()->in_direction(WLR_DIRECTION_RIGHT);
        if (right)
            output->get_active()->focus_toplevel(right);
    } else if (bind == config->window_close) {
        // close the active toplevel
        output->get_active()->close_active();
    } else if (bind == config->window_swap_up) {
        // swap the active toplevel with the one above it
        Toplevel *other = output->get_active()->in_direction(WLR_DIRECTION_UP);
        if (other)
            output->get_active()->swap(other);
    } else if (bind == config->window_swap_down) {
        // swap the active toplevel with the one below it
        Toplevel *other =
            output->get_active()->in_direction(WLR_DIRECTION_DOWN);
        if (other)
            output->get_active()->swap(other);
    } else if (bind == config->window_swap_left) {
        // swap the active toplevel with the one to the left of it
        Toplevel *other =
            output->get_active()->in_direction(WLR_DIRECTION_LEFT);
        if (other)
            output->get_active()->swap(other);
    } else if (bind == config->window_swap_right) {
        // swap the active toplevel with the one to the right of it
        Toplevel *other =
            output->get_active()->in_direction(WLR_DIRECTION_RIGHT);
        if (other)
            output->get_active()->swap(other);
    } else if (bind == config->workspace_tile) {
        // set workspace to tile
        output->get_active()->tile();
    } else if (bind.sym >= XKB_KEY_0 && bind.sym <= XKB_KEY_9) {
        // digit pressed
        Bind digbind = Bind{bind.modifiers, XKB_KEY_NoSymbol};

        // 0 is on the right of 9 so it makes more sense this way
        int n = XKB_KEY_0 == bind.sym ? 10 : bind.sym - XKB_KEY_0 - 1;

        if (digbind == config->workspace_open) {
            // set workspace to n
            return output->set_workspace(n);
        } else if (digbind == config->workspace_window_to) {
            // move active toplevel to workspace n
            Workspace *current = output->get_active();
            Workspace *target = output->get_workspace(n);

            if (target == nullptr)
                return false;

            if (current->active_toplevel)
                current->move_to(current->active_toplevel, target);
        } else
            return false;
    } else
        return false;

    return true;
}

// update the keyboard config
void Keyboard::update_config() {
    // create xkb context
    struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

    // get config
    Config *config = server->config;

    // create rule names from config
    struct xkb_rule_names names{
        .rules = NULL,
        .model = config->keyboard_model.data(),
        .layout = config->keyboard_layout.data(),
        .variant = config->keyboard_variant.data(),
        .options = config->keyboard_options.data(),
    };

    // keymap
    struct xkb_keymap *keymap{NULL};
    if (!(keymap = xkb_keymap_new_from_names(context, &names,
                                             XKB_KEYMAP_COMPILE_NO_FLAGS))) {
        notify_send(
            "failed to load keymap - layout: %s, model: %s, variant: %s",
            names.layout, names.model, names.variant);

        // load default keymap
        keymap = xkb_keymap_new_from_names(context, NULL,
                                           XKB_KEYMAP_COMPILE_NO_FLAGS);
    }

    // set keymap
    wlr_keyboard_set_keymap(wlr_keyboard, keymap);
    xkb_keymap_unref(keymap);
    xkb_context_unref(context);

    // repeat info
    wlr_keyboard_set_repeat_info(wlr_keyboard, server->config->repeat_rate,
                                 server->config->repeat_delay);
}

// get keysyms without modifiers applied
uint32_t Keyboard::keysyms_raw(xkb_keycode_t keycode,
                               const xkb_keysym_t **keysyms,
                               uint32_t *modifiers) {
    *modifiers = wlr_keyboard_get_modifiers(wlr_keyboard);

    xkb_layout_index_t layout_index =
        xkb_state_key_get_layout(wlr_keyboard->xkb_state, keycode);
    return xkb_keymap_key_get_syms_by_level(wlr_keyboard->keymap, keycode,
                                            layout_index, 0, keysyms);
}

// get keysyms with modifiers applied
uint32_t Keyboard::keysyms_translated(xkb_keycode_t keycode,
                                      const xkb_keysym_t **keysyms,
                                      uint32_t *modifiers) {
    *modifiers = wlr_keyboard_get_modifiers(wlr_keyboard);

    xkb_mod_mask_t consumed = xkb_state_key_get_consumed_mods2(
        wlr_keyboard->xkb_state, keycode, XKB_CONSUMED_MODE_XKB);
    *modifiers &= ~consumed;

    return xkb_state_key_get_syms(wlr_keyboard->xkb_state, keycode, keysyms);
}

Keyboard::Keyboard(struct Server *server, struct wlr_input_device *device) {
    this->server = server;
    this->wlr_keyboard = wlr_keyboard_from_input_device(device);

    // set data
    wlr_keyboard->data = this;

    // set config
    update_config();

    // handle_modifiers
    modifiers.notify = [](struct wl_listener *listener, void *data) {
        struct Keyboard *keyboard =
            wl_container_of(listener, keyboard, modifiers);

        // set seat keyboard
        wlr_seat_set_keyboard(keyboard->server->seat, keyboard->wlr_keyboard);

        // send mods to seat
        wlr_seat_keyboard_notify_modifiers(keyboard->server->seat,
                                           &keyboard->wlr_keyboard->modifiers);
    };
    wl_signal_add(&wlr_keyboard->events.modifiers, &modifiers);

    // handle_key
    key.notify = [](struct wl_listener *listener, void *data) {
        // key is pressed or released
        struct Keyboard *keyboard = wl_container_of(listener, keyboard, key);
        struct Server *server = keyboard->server;
        struct wlr_keyboard_key_event *event =
            static_cast<wlr_keyboard_key_event *>(data);
        struct wlr_seat *seat = server->seat;

        // libinput keycode -> xkbcommon
        uint32_t keycode = event->keycode + 8;

        // handle binds
        bool handled = false;

        if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
            // modifiers
            uint32_t modifiers;

            // raw
            const xkb_keysym_t *syms_raw;
            uint32_t nsyms_raw =
                keyboard->keysyms_raw(keycode, &syms_raw, &modifiers);

            for (uint32_t i = 0; i != nsyms_raw; ++i)
                handled = keyboard->handle_bind(Bind{modifiers, syms_raw[i]},
                                                event->keycode);

            // translated
            const xkb_keysym_t *syms_translated;
            uint32_t nsyms_translated = keyboard->keysyms_translated(
                keycode, &syms_translated, &modifiers);

            for (uint32_t i = 0; i != nsyms_translated; ++i)
                handled = keyboard->handle_bind(
                    Bind{modifiers, syms_translated[i]}, event->keycode);
        }

        if (!handled) {
            // send unhandled keypresses to seat
            wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);
            wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode,
                                         event->state);
        }
    };
    wl_signal_add(&wlr_keyboard->events.key, &key);

    // handle_destroy
    destroy.notify = [](struct wl_listener *listener, void *data) {
        struct Keyboard *keyboard =
            wl_container_of(listener, keyboard, destroy);
        delete keyboard;
    };
    wl_signal_add(&device->events.destroy, &destroy);
}

Keyboard::~Keyboard() {
    wl_list_remove(&modifiers.link);
    wl_list_remove(&key.link);
    wl_list_remove(&destroy.link);
    wl_list_remove(&link);
}
