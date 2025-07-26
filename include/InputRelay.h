#pragma once

#include "wlr.h"

struct InputRelay {
    struct Seat *seat;
    wlr_scene_tree *popup_tree;

    struct InputMethod *input_method{nullptr};

    wl_list text_inputs;
    struct TextInput *active_text_input;

    wl_list popups;
    wlr_surface *focused_surface{nullptr};

    wl_listener new_text_input;
    wl_listener new_input_method;
    wl_listener focused_surface_destroy;
    wl_listener keyboard_grab_destroy;

    InputRelay(struct Seat *seat);
    ~InputRelay();

    TextInput *get_active_text_input();
    void update_active_text_input();
    void update_text_inputs_focused_surface();
    void update_popups_positions();
    void send_state_to_input_method();
    void set_focus(wlr_surface *surface);
};
