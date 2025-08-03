#include "Keyboard.h"
#include "Config.h"
#include "Cursor.h"
#include "IPC.h"
#include "Seat.h"
#include "Server.h"
#include "util.h"

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
    xkb_keymap *keymap =
        xkb_keymap_new_from_names(context, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!keymap) {
        notify_send(
            "Config",
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

    // notify clients
    if (server->ipc)
        server->ipc->notify_clients({IPC_KEYBOARD_LIST, IPC_DEVICE_CURRENT});
}

// get keysyms without modifiers applied
uint32_t Keyboard::keysyms_raw(const xkb_keycode_t keycode,
                               const xkb_keysym_t **keysyms) const {
    const xkb_layout_index_t layout_index =
        xkb_state_key_get_layout(wlr_keyboard->xkb_state, keycode);
    return xkb_keymap_key_get_syms_by_level(wlr_keyboard->keymap, keycode,
                                            layout_index, 0, keysyms);
}

// get keysyms with modifiers applied
uint32_t Keyboard::keysyms_translated(const xkb_keycode_t keycode,
                                      const xkb_keysym_t **keysyms) const {
    return xkb_state_key_get_syms(wlr_keyboard->xkb_state, keycode, keysyms);
}

Keyboard::Keyboard(Server *server, struct wlr_keyboard *keyboard)
    : server(server), wlr_keyboard(keyboard) {
    // connect to seat
    wlr_seat_set_keyboard(server->seat->wlr_seat, keyboard);

    // add to keyboards list
    wl_list_insert(&server->keyboards, &link);

    // set data
    wlr_keyboard->data = this;

    // set config
    update_config();

    // handle_modifiers
    modifiers.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        Keyboard *keyboard = wl_container_of(listener, keyboard, modifiers);

        // set seat keyboard
        wlr_seat_set_keyboard(keyboard->server->seat->wlr_seat,
                              keyboard->wlr_keyboard);

        // send mods to seat
        wlr_seat_keyboard_notify_modifiers(keyboard->server->seat->wlr_seat,
                                           &keyboard->wlr_keyboard->modifiers);
        // TODO: Hacky way to detect layout switch in IPC
        // should be triggered on layout switch
        if (IPC *ipc = keyboard->server->ipc)
            ipc->notify_clients(IPC_DEVICE_CURRENT);
    };
    wl_signal_add(&wlr_keyboard->events.modifiers, &modifiers);

    // handle_key
    key.notify = [](wl_listener *listener, void *data) {
        // key is pressed or released
        Keyboard *keyboard = wl_container_of(listener, keyboard, key);
        Server *server = keyboard->server;
        const auto *event = static_cast<wlr_keyboard_key_event *>(data);
        wlr_seat *seat = server->seat->wlr_seat;

        // notify activity
        wlr_idle_notifier_v1_notify_activity(server->wlr_idle_notifier, seat);

        // libinput keycode -> xkbcommon
        const uint32_t keycode = event->keycode + 8;

        // handle binds
        bool handled = false;

        // handle vt switch
        auto handle_vt = [&](const xkb_keysym_t *keysyms, size_t len) {
            for (size_t i = 0; i != len; ++i) {
                xkb_keysym_t keysym = keysyms[i];

                if (keysym >= XKB_KEY_XF86Switch_VT_1 &&
                    keysym <= XKB_KEY_XF86Switch_VT_12) {
                    if (server->session)
                        // change vt to the specified number
                        wlr_session_change_vt(
                            server->session,
                            (unsigned int)(keysym + 1 -
                                           XKB_KEY_XF86Switch_VT_1));
                    return true;
                }
            }

            return false;
        };

        if (!server->locked && event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
            // modifiers
            uint32_t modifiers =
                wlr_keyboard_get_modifiers(keyboard->wlr_keyboard);

            // raw
            const xkb_keysym_t *syms_raw;
            uint32_t nsyms_raw = keyboard->keysyms_raw(keycode, &syms_raw);

            handled |= handle_vt(syms_raw, nsyms_raw);

            for (uint32_t i = 0; i != nsyms_raw; ++i)
                handled |= server->handle_bind(
                    Bind{BIND_NONE, modifiers, syms_raw[i]});

            // translated
            const xkb_keysym_t *syms_translated;
            uint32_t nsyms_translated =
                keyboard->keysyms_translated(keycode, &syms_translated);

            handled |= handle_vt(syms_translated, nsyms_translated);

            if (modifiers & (WLR_MODIFIER_SHIFT | WLR_MODIFIER_CAPS))
                for (uint32_t i = 0; i != nsyms_translated; ++i)
                    handled |= server->handle_bind(
                        Bind{BIND_NONE, modifiers, syms_translated[i]});
        }

        if (!handled) {
            // send unhandled key presses to seat
            wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);
            wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode,
                                         event->state);
        }
    };
    wl_signal_add(&wlr_keyboard->events.key, &key);

    // destroy signal for keyboards is set in server constructor because it
    // differs between virtual and non-virtual keyboards
    destroy.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        Keyboard *keyboard = wl_container_of(listener, keyboard, destroy);
        delete keyboard;
    };
}

Keyboard::~Keyboard() {
    wl_list_remove(&link);
    wl_list_remove(&modifiers.link);
    wl_list_remove(&key.link);
    wl_list_remove(&destroy.link);

    // notify clients
    if (server->ipc)
        server->ipc->notify_clients(
            {IPC_KEYBOARD_LIST, IPC_DEVICE_LIST, IPC_DEVICE_CURRENT});
}
