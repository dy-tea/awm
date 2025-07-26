#include "TextInput.h"
#include "InputRelay.h"

// based on labwc implementation

TextInput::TextInput(InputRelay *relay, wlr_text_input_v3 *wlr_text_input)
    : relay(relay), wlr_text_input(wlr_text_input) {

    wl_list_insert(&relay->text_inputs, &link);

    // enable
    enable.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        TextInput *text_input = wl_container_of(listener, text_input, enable);
        InputRelay *relay = text_input->relay;

        relay->update_active_text_input();
        if (relay->active_text_input == text_input) {
            relay->update_popups_positions();
            relay->send_state_to_input_method();
        }
    };
    wl_signal_add(&wlr_text_input->events.enable, &enable);

    // disable
    disable.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        TextInput *text_input = wl_container_of(listener, text_input, disable);
        text_input->relay->update_active_text_input();
    };
    wl_signal_add(&wlr_text_input->events.disable, &disable);

    // commit
    commit.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        TextInput *text_input = wl_container_of(listener, text_input, commit);
        InputRelay *relay = text_input->relay;

        if (relay->active_text_input == text_input) {
            relay->update_popups_positions();
            relay->send_state_to_input_method();
        }
    };
    wl_signal_add(&wlr_text_input->events.commit, &commit);

    // destroy
    destroy.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        TextInput *text_input = wl_container_of(listener, text_input, destroy);
        delete text_input;
    };
    wl_signal_add(&wlr_text_input->events.destroy, &destroy);

    relay->update_text_inputs_focused_surface();
}

TextInput::~TextInput() {
    wl_list_remove(&enable.link);
    wl_list_remove(&commit.link);
    wl_list_remove(&disable.link);
    wl_list_remove(&destroy.link);
    wl_list_remove(&link);

    relay->update_active_text_input();
}
