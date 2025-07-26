#include "InputMethod.h"
#include "InputMethodPopup.h"
#include "InputRelay.h"
#include "Seat.h"
#include "TextInput.h"
#include "wlr.h"

// based on labwc implementation

InputMethod::InputMethod(InputRelay *relay,
                         wlr_input_method_v2 *wlr_input_method)
    : relay(relay), wlr_input_method(wlr_input_method) {

    commit.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        InputMethod *input_method =
            wl_container_of(listener, input_method, commit);
        wlr_input_method_v2_state *current =
            &input_method->wlr_input_method->current;

        TextInput *text_input = input_method->relay->active_text_input;
        if (!text_input)
            return;

        wlr_text_input_v3 *input = text_input->wlr_text_input;

        if (current->preedit.text)
            wlr_text_input_v3_send_preedit_string(input, current->preedit.text,
                                                  current->preedit.cursor_begin,
                                                  current->preedit.cursor_end);

        if (current->commit_text)
            wlr_text_input_v3_send_commit_string(input, current->commit_text);

        if (current->delete_.before_length || current->delete_.after_length)
            wlr_text_input_v3_send_delete_surrounding_text(
                input, current->delete_.before_length,
                current->delete_.after_length);

        wlr_text_input_v3_send_done(input);
    };
    wl_signal_add(&wlr_input_method->events.commit, &commit);

    grab_keyboard.notify = [](wl_listener *listener, void *data) {
        InputMethod *input_method =
            wl_container_of(listener, input_method, grab_keyboard);
        InputRelay *relay = input_method->relay;
        auto *keyboard_grab =
            static_cast<wlr_input_method_keyboard_grab_v2 *>(data);

        wlr_keyboard *active_keyboard =
            wlr_seat_get_keyboard(relay->seat->wlr_seat);

        if (!input_method->is_keyboard_emulated_by_input_method(
                active_keyboard))
            wlr_input_method_keyboard_grab_v2_set_keyboard(keyboard_grab,
                                                           active_keyboard);

        relay->keyboard_grab_destroy.notify = [](wl_listener *listener,
                                                 void *data) {
            InputRelay *relay =
                wl_container_of(listener, relay, keyboard_grab_destroy);
            auto keyboard_grab =
                static_cast<wlr_input_method_keyboard_grab_v2 *>(data);

            wl_list_remove(&relay->keyboard_grab_destroy.link);

            if (keyboard_grab->keyboard)
                wlr_seat_keyboard_notify_modifiers(
                    keyboard_grab->input_method->seat,
                    &keyboard_grab->keyboard->modifiers);
        };
        wl_signal_add(&keyboard_grab->events.destroy,
                      &relay->keyboard_grab_destroy);
    };
    wl_signal_add(&wlr_input_method->events.grab_keyboard, &grab_keyboard);

    new_popup_surface.notify = [](wl_listener *listener, void *data) {
        InputMethod *input_method =
            wl_container_of(listener, input_method, new_popup_surface);
        new InputMethodPopup(input_method->relay,
                             static_cast<wlr_input_popup_surface_v2 *>(data));
    };
    wl_signal_add(&wlr_input_method->events.new_popup_surface,
                  &new_popup_surface);

    destroy.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        InputMethod *input_method =
            wl_container_of(listener, input_method, destroy);
        delete input_method;
    };
    wl_signal_add(&wlr_input_method->events.destroy, &destroy);

    relay->update_text_inputs_focused_surface();
    relay->update_active_text_input();
}

InputMethod::~InputMethod() {
    wl_list_remove(&commit.link);
    wl_list_remove(&grab_keyboard.link);
    wl_list_remove(&new_popup_surface.link);
    wl_list_remove(&destroy.link);
    relay->input_method = nullptr;

    relay->update_text_inputs_focused_surface();
    relay->update_active_text_input();
}

bool InputMethod::is_keyboard_emulated_by_input_method(
    wlr_keyboard *keyboard) const {
    if (!keyboard || !wlr_input_method)
        return false;

    wlr_virtual_keyboard_v1 *virtual_keyboard =
        wlr_input_device_get_virtual_keyboard(&keyboard->base);
    return virtual_keyboard &&
           wl_resource_get_client(virtual_keyboard->resource) ==
               wl_resource_get_client(wlr_input_method->resource);
}
