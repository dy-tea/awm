#include "Server.h"

bool Keyboard::handle_bind(struct Bind bind, uint32_t keycode) {
    // retrieve config
    Config *config = server->config;

    // get current output
    Output *output = server->output_at(server->cursor->x, server->cursor->y);
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
    } else if (bind == config->workspace_tile) {
        // set workspace to tile
        output->get_active()->tile();
    } else if (keycode >= 2 && keycode <= 11) {
        // digit pressed
        Bind digbind = Bind{bind.modifiers, XKB_KEY_NoSymbol};

        if (digbind == config->workspace_open) {
            // set workspace to n
            return output->set_workspace(keycode - 2);
        } else if (digbind == config->workspace_window_to) {
            // move active toplevel to workspace n
            Workspace *current = output->get_active();
            Workspace *target = output->get_workspace(keycode - 2);

            if (target == nullptr)
                return false;

            current->move_to(current->active_toplevel, target);
        } else
            return false;
    } else
        return false;

    return true;
}

Keyboard::Keyboard(struct Server *server, struct wlr_input_device *device) {
    this->server = server;
    this->wlr_keyboard = wlr_keyboard_from_input_device(device);

    /* We need to prepare an XKB keymap and assign it to the keyboard. This
     * assumes the defaults (e.g. layout = "us"). */
    struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap *keymap =
        xkb_keymap_new_from_names(context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);

    wlr_keyboard_set_keymap(wlr_keyboard, keymap);
    xkb_keymap_unref(keymap);
    xkb_context_unref(context);
    wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);

    /* Here we set up listeners for keyboard events. */

    // handle_modifiers
    modifiers.notify = [](struct wl_listener *listener, void *data) {
        /* This event is raised when a modifier key, such as shift or alt,
         * is pressed. We simply communicate this to the client. */
        struct Keyboard *keyboard =
            wl_container_of(listener, keyboard, modifiers);
        /*
         * A seat can only have one keyboard, but this is a limitation of
         * the Wayland protocol - not wlroots. We assign all connected
         * keyboards to the same seat. You can swap out the underlying
         * wlr_keyboard like this and wlr_seat handles this transparently.
         */
        wlr_seat_set_keyboard(keyboard->server->seat, keyboard->wlr_keyboard);
        /* Send modifiers to the client. */
        wlr_seat_keyboard_notify_modifiers(keyboard->server->seat,
                                           &keyboard->wlr_keyboard->modifiers);
    };
    wl_signal_add(&wlr_keyboard->events.modifiers, &modifiers);

    // handle_key (keyboard)
    key.notify = [](struct wl_listener *listener, void *data) {
        /* This event is raised when a key is pressed or released. */
        struct Keyboard *keyboard = wl_container_of(listener, keyboard, key);
        struct Server *server = keyboard->server;
        struct wlr_keyboard_key_event *event = (wlr_keyboard_key_event *)data;
        struct wlr_seat *seat = server->seat;

        /* Translate libinput keycode -> xkbcommon */
        uint32_t keycode = event->keycode + 8;
        /* Get a list of keysyms based on the keymap for this keyboard */
        const xkb_keysym_t *syms;
        int nsyms = xkb_state_key_get_syms(keyboard->wlr_keyboard->xkb_state,
                                           keycode, &syms);

        bool handled = false;
        uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->wlr_keyboard);

        // handle binds
        if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED)
            for (int i = 0; i != nsyms; ++i)
                handled = keyboard->handle_bind(Bind{modifiers, syms[i]},
                                                event->keycode);

        if (!handled) {
            /* Otherwise, we pass it along to the client. */
            wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);
            wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode,
                                         event->state);
        }
    };
    wl_signal_add(&wlr_keyboard->events.key, &key);

    // handle_destroy (keyboard)
    destroy.notify = [](struct wl_listener *listener, void *data) {
        /* This event is raised by the keyboard base wlr_input_device to
         * signal the destruction of the wlr_keyboard. It will no longer
         * receive events and should be destroyed.
         */
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
