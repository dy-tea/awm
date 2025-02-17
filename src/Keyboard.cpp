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
        notify_send("failed to create keymap");
        notify_send("layout: %s, model: %s, variant: %s", names.layout,
                    names.model, names.variant);
        xkb_context_unref(context);
        return;
    }

    // set keymap
    wlr_keyboard_set_keymap(wlr_keyboard, keymap);
    xkb_keymap_unref(keymap);
    xkb_context_unref(context);

    // repeat info
    wlr_keyboard_set_repeat_info(wlr_keyboard, server->config->repeat_rate,
                                 server->config->repeat_delay);
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

        // keysym list based on keymap
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
