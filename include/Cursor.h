#pragma once

#include "wlr.h"
#include <vector>

enum CursorMode {
    CURSORMODE_PASSTHROUGH,
    CURSORMODE_MOVE,
    CURSORMODE_RESIZE,
};

enum CursorButton {
    CURSOR_BUTTON_LEFT = 1 << 0,
    CURSOR_BUTTON_RIGHT = 1 << 1,
    CURSOR_BUTTON_MIDDLE = 1 << 2,
    CURSOR_BUTTON_BACK = 1 << 3,
    CURSOR_BUTTON_FORWARD = 1 << 4,
};

struct Cursor {
    struct Server *server;
    wlr_cursor *cursor;
    wlr_xcursor_manager *xcursor_manager;
    wlr_seat *seat;
    CursorMode cursor_mode;

    std::vector<wlr_pointer *> pointers;

    double grab_x, grab_y;
    wlr_box grab_geobox;
    uint32_t resize_edges;
    uint32_t pressed_buttons{0};

    wl_listener motion;
    wl_listener motion_absolute;
    wl_listener button;
    wl_listener axis;
    wl_listener frame;

    wlr_cursor_shape_manager_v1 *wlr_cursor_shape_manager;
    wl_listener request_set_shape;

    wlr_pointer_constraint_v1 *active_constraint{nullptr};
    bool requires_warp{false};
    pixman_region32_t confine;
    wl_listener constraint_commit;

    wlr_pointer_gestures_v1 *wlr_pointer_gestures;
    wl_listener pinch_begin;
    wl_listener pinch_update;
    wl_listener pinch_end;
    wl_listener swipe_begin;
    wl_listener swipe_update;
    wl_listener swipe_end;
    wl_listener hold_begin;
    wl_listener hold_end;

    Cursor(struct Seat *seat);
    ~Cursor();

    void reset_mode();
    void notify_activity();
    void process_motion(uint32_t time, wlr_input_device *device, double dx,
                        double dy, double unaccel_dx, double unaccel_dy);
    void process_move();
    void process_resize();
    void constrain(wlr_pointer_constraint_v1 *constraint);
    void check_constraint_region();
    void warp_to_constraint_hint();

    bool is_touchpad(wlr_pointer *pointer) const;
    void set_config(wlr_pointer *pointer);
    void reconfigure_all();
};
