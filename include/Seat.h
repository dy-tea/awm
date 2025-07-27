#pragma once

#include "wlr.h"

struct Seat {
    struct Server *server;

    struct wlr_seat *wlr_seat;
    wl_listener new_input;
    wl_listener pointer_focus_change;
    wl_listener request_set_cursor;
    wl_listener request_set_selection;
    wl_listener request_set_primary_selection;
    wl_listener request_start_drag;
    wl_listener start_drag;
    wl_listener destroy_drag_icon;

    struct Toplevel *grabbed_toplevel{nullptr};

    wlr_virtual_pointer_manager_v1 *virtual_pointer_mgr;
    wl_listener new_virtual_pointer;

    wlr_virtual_keyboard_manager_v1 *virtual_keyboard_manager;
    wl_listener new_virtual_keyboard;

    struct InputRelay *input_relay;

    Seat(Server *server);
    ~Seat();
};
