#include "InputRelay.h"
#include "InputMethod.h"
#include "InputMethodPopup.h"
#include "Seat.h"
#include "Server.h"
#include "TextInput.h"
#include "wlr.h"

// based on labwc implementation

InputRelay::InputRelay(Seat *seat)
    : seat(seat),
      popup_tree(wlr_scene_tree_create(&seat->server->scene->tree)) {
    wl_list_init(&text_inputs);
    wl_list_init(&popups);

    new_text_input.notify = [](wl_listener *listener, void *data) {
        InputRelay *relay = wl_container_of(listener, relay, new_text_input);
        auto *text_input = static_cast<wlr_text_input_v3 *>(data);
        if (relay->seat->wlr_seat != text_input->seat)
            return;

        new TextInput(relay, text_input);
    };
    wl_signal_add(&seat->server->wlr_text_input_manager->events.new_text_input,
                  &new_text_input);

    new_input_method.notify = [](wl_listener *listener, void *data) {
        InputRelay *relay = wl_container_of(listener, relay, new_input_method);
        auto *input_method = static_cast<wlr_input_method_v2 *>(data);
        if (relay->seat->wlr_seat != input_method->seat)
            return;

        if (relay->input_method) {
            wlr_log(WLR_INFO, "%s", "duplicate input method received for seat");
            return;
        }

        relay->input_method = new InputMethod(relay, input_method);
    };
    wl_signal_add(&seat->server->wlr_input_method_manager->events.input_method,
                  &new_input_method);

    focused_surface_destroy.notify = [](wl_listener *listener,
                                        [[maybe_unused]] void *data) {
        InputRelay *relay =
            wl_container_of(listener, relay, focused_surface_destroy);
        relay->set_focus(nullptr);
    };
    // signal is added in set_focus
}

InputRelay::~InputRelay() {
    wl_list_remove(&new_text_input.link);
    wl_list_remove(&new_input_method.link);
}

TextInput *InputRelay::get_active_text_input() {
    if (!input_method)
        return nullptr;

    TextInput *input, *tmp;
    wl_list_for_each_safe(
        input, tmp, &text_inputs,
        link) if (input->wlr_text_input->focused_surface &&
                  input->wlr_text_input->current_enabled) return input;

    return nullptr;
}

void InputRelay::update_active_text_input() {
    TextInput *active = get_active_text_input();

    if (input_method && active_text_input != active) {
        wlr_input_method_v2 *im = input_method->wlr_input_method;
        if (active_text_input)
            wlr_input_method_v2_send_activate(im);
        else
            wlr_input_method_v2_send_deactivate(im);
        wlr_input_method_v2_send_done(im);
    }

    active_text_input = active;
}

void InputRelay::update_text_inputs_focused_surface() {
    TextInput *text_input, *tmp;
    wl_list_for_each_safe(text_input, tmp, &text_inputs, link) {
        wlr_text_input_v3 *input = text_input->wlr_text_input;

        wlr_surface *new_focus =
            input_method && focused_surface &&
                    input->focused_surface == focused_surface
                ? focused_surface
                : nullptr;

        if (input->focused_surface == new_focus)
            continue;

        if (input->focused_surface)
            wlr_text_input_v3_send_leave(input);
        if (new_focus)
            wlr_text_input_v3_send_enter(input, new_focus);
    }
}

void InputRelay::update_popups_positions() {
    InputMethodPopup *popup, *tmp;
    wl_list_for_each_safe(popup, tmp, &popups, link) popup->update_position();
}

void InputRelay::send_state_to_input_method() {
    wlr_text_input_v3 *input = active_text_input->wlr_text_input;

    if (input->active_features & WLR_TEXT_INPUT_V3_FEATURE_SURROUNDING_TEXT)
        wlr_input_method_v2_send_surrounding_text(
            input_method->wlr_input_method, input->current.surrounding.text,
            input->current.surrounding.cursor,
            input->current.surrounding.anchor);

    wlr_input_method_v2_send_text_change_cause(
        input_method->wlr_input_method, input->current.text_change_cause);

    if (input->active_features & WLR_TEXT_INPUT_V3_FEATURE_CONTENT_TYPE)
        wlr_input_method_v2_send_content_type(
            input_method->wlr_input_method, input->current.content_type.hint,
            input->current.content_type.purpose);

    wlr_input_method_v2_send_done(input_method->wlr_input_method);
}

void InputRelay::set_focus(wlr_surface *surface) {
    if (focused_surface == surface)
        return;

    if (focused_surface)
        wl_list_remove(&focused_surface_destroy.link);

    focused_surface = surface;

    if (surface)
        wl_signal_add(&surface->events.destroy, &focused_surface_destroy);

    update_text_inputs_focused_surface();
    update_active_text_input();
}
