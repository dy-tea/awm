#include "Server.h"

TextInput::TextInput(Server *server, wlr_text_input_v3 *wlr_text_input)
    : server(server), wlr_text_input(wlr_text_input) {

    // enable
    enable.notify = [](wl_listener *listener, void *data) {
        TextInput *text_input = wl_container_of(listener, text_input, enable);
        wlr_text_input_v3 *wlr_text_input =
            static_cast<wlr_text_input_v3 *>(data);

        wlr_text_input->current_enabled = true;
    };
    wl_signal_add(&wlr_text_input->events.enable, &enable);

    // commit
    commit.notify = [](wl_listener *listener, void *data) {
        TextInput *text_input = wl_container_of(listener, text_input, commit);
        wlr_text_input_v3 *wlr_text_input =
            static_cast<wlr_text_input_v3 *>(data);

        if (!(wlr_text_input || wlr_text_input->current_enabled))
            return;

        wlr_text_input_v3_send_done(wlr_text_input);
    };
    wl_signal_add(&wlr_text_input->events.commit, &commit);

    // disable
    disable.notify = [](wl_listener *listener, void *data) {
        TextInput *text_input = wl_container_of(listener, text_input, disable);
        wlr_text_input_v3 *wlr_text_input =
            static_cast<wlr_text_input_v3 *>(data);

        if (!wlr_text_input->focused_surface)
            return;

        wlr_text_input->current_enabled = false;
    };
    wl_signal_add(&wlr_text_input->events.disable, &disable);

    // destroy
    destroy.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        TextInput *text_input = wl_container_of(listener, text_input, destroy);
        delete text_input;
    };
    wl_signal_add(&wlr_text_input->events.destroy, &destroy);
}

TextInput::~TextInput() {
    wl_list_remove(&enable.link);
    wl_list_remove(&commit.link);
    wl_list_remove(&disable.link);
    wl_list_remove(&destroy.link);
}
