#include "Server.h"

// execute either a wm bind or command bind, returns true if
// bind is valid, false otherwise
bool Keyboard::handle_bind(const Bind bind) {
    // retrieve config
    Config *config = server->config;

    // get current output
    Output *output = server->focused_output();
    if (!output)
        return false;

    // handle user-defined binds
    for (const auto &[cmd_bind, cmd] : config->commands)
        if (cmd_bind == bind)
            if (fork() == 0) {
                execl("/bin/sh", "/bin/sh", "-c", cmd.c_str(), nullptr);
                return true;
            }

    // handle compositor binds
    if (bind == config->exit) {
        // exit compositor
        server->exit();
    } else if (bind == config->window_fullscreen) {
        // fullscreen the active toplevel
        Toplevel *active = output->get_active()->active_toplevel;

        if (active == nullptr)
            return false;

        active->toggle_fullscreen();
    } else if (bind == config->window_previous) {
        // focus the previous toplevel in the active workspace
        output->get_active()->focus_prev();
    } else if (bind == config->window_next) {
        // focus the next toplevel in the active workspace
        output->get_active()->focus_next();
    } else if (bind == config->window_move) {
        // move the active toplevel with the mouse
        if (Toplevel *active = output->get_active()->active_toplevel)
            active->begin_interactive(CURSORMODE_MOVE, 0);
    } else if (bind == config->window_up) {
        // focus the toplevel in the up direction
        if (Toplevel *up = output->get_active()->in_direction(WLR_DIRECTION_UP))
            output->get_active()->focus_toplevel(up);
    } else if (bind == config->window_down) {
        // focus the toplevel in the down direction
        if (Toplevel *down =
                output->get_active()->in_direction(WLR_DIRECTION_DOWN))
            output->get_active()->focus_toplevel(down);
    } else if (bind == config->window_left) {
        // focus the toplevel in the left direction
        if (Toplevel *left =
                output->get_active()->in_direction(WLR_DIRECTION_LEFT))
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
        if (Toplevel *other =
                output->get_active()->in_direction(WLR_DIRECTION_UP))
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
        const Bind digit_bind = Bind{bind.modifiers, XKB_KEY_NoSymbol};

        // 0 is on the right of 9 so it makes more sense this way
        const int n = XKB_KEY_0 == bind.sym ? 10 : bind.sym - XKB_KEY_0 - 1;

        if (digit_bind == config->workspace_open) {
            // set workspace to n
            return output->set_workspace(n);
        } else if (digit_bind == config->workspace_window_to) {
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
void Keyboard::update_config() const {
    // create xkb context
    xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

    // get config
    Config *config = server->config;

    // create rule names from config
    const xkb_rule_names names{
        nullptr,
        config->keyboard_model.data(),
        config->keyboard_layout.data(),
        config->keyboard_variant.data(),
        config->keyboard_options.data(),
    };

    // keymap
    xkb_keymap *keymap{nullptr};
    if (!((keymap = xkb_keymap_new_from_names(context, &names,
                                              XKB_KEYMAP_COMPILE_NO_FLAGS)))) {
        notify_send(
            "failed to load keymap - layout: %s, model: %s, variant: %s",
            names.layout, names.model, names.variant);

        // load default keymap
        keymap = xkb_keymap_new_from_names(context, nullptr,
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
uint32_t Keyboard::keysyms_raw(const xkb_keycode_t keycode,
                               const xkb_keysym_t **keysyms,
                               uint32_t *modifiers) const {
    *modifiers = wlr_keyboard_get_modifiers(wlr_keyboard);

    const xkb_layout_index_t layout_index =
        xkb_state_key_get_layout(wlr_keyboard->xkb_state, keycode);
    return xkb_keymap_key_get_syms_by_level(wlr_keyboard->keymap, keycode,
                                            layout_index, 0, keysyms);
}

// get keysyms with modifiers applied
uint32_t Keyboard::keysyms_translated(const xkb_keycode_t keycode,
                                      const xkb_keysym_t **keysyms,
                                      uint32_t *modifiers) const {
    *modifiers = wlr_keyboard_get_modifiers(wlr_keyboard);

    const xkb_mod_mask_t consumed = xkb_state_key_get_consumed_mods2(
        wlr_keyboard->xkb_state, keycode, XKB_CONSUMED_MODE_XKB);
    *modifiers &= ~consumed;

    return xkb_state_key_get_syms(wlr_keyboard->xkb_state, keycode, keysyms);
}

Keyboard::Keyboard(Server *server, wlr_input_device *device)
    : server(server), wlr_keyboard(wlr_keyboard_from_input_device(device)) {
    // set data
    wlr_keyboard->data = this;

    // set config
    update_config();

    // handle_modifiers
    modifiers.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        Keyboard *keyboard = wl_container_of(listener, keyboard, modifiers);

        // set seat keyboard
        wlr_seat_set_keyboard(keyboard->server->seat, keyboard->wlr_keyboard);

        // send mods to seat
        wlr_seat_keyboard_notify_modifiers(keyboard->server->seat,
                                           &keyboard->wlr_keyboard->modifiers);
    };
    wl_signal_add(&wlr_keyboard->events.modifiers, &modifiers);

    // handle_key
    key.notify = [](wl_listener *listener, void *data) {
        // key is pressed or released
        Keyboard *keyboard = wl_container_of(listener, keyboard, key);
        const Server *server = keyboard->server;
        const auto *event = static_cast<wlr_keyboard_key_event *>(data);
        wlr_seat *seat = server->seat;

        // libinput keycode -> xkbcommon
        const uint32_t keycode = event->keycode + 8;

        // handle binds
        bool handled = false;

        if (!server->locked && event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
            // modifiers
            uint32_t modifiers = 0;

            // raw
            const xkb_keysym_t *syms_raw;
            uint32_t nsyms_raw =
                keyboard->keysyms_raw(keycode, &syms_raw, &modifiers);

            for (uint32_t i = 0; i != nsyms_raw; ++i)
                handled = keyboard->handle_bind(Bind{modifiers, syms_raw[i]});

            // translated
            const xkb_keysym_t *syms_translated;
            uint32_t nsyms_translated = keyboard->keysyms_translated(
                keycode, &syms_translated, &modifiers);

            if (modifiers & WLR_MODIFIER_SHIFT || modifiers & WLR_MODIFIER_CAPS)
                for (uint32_t i = 0; i != nsyms_translated; ++i)
                    handled = keyboard->handle_bind(
                        Bind{modifiers, syms_translated[i]});
        }

        if (!handled) {
            // send unhandled key presses to seat
            wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);
            wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode,
                                         event->state);
        }
    };
    wl_signal_add(&wlr_keyboard->events.key, &key);

    // handle_destroy
    destroy.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        Keyboard *keyboard = wl_container_of(listener, keyboard, destroy);
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
