#include "wlr.h"
#include <vector>

enum CursorMode {
    CURSORMODE_PASSTHROUGH,
    CURSORMODE_MOVE,
    CURSORMODE_RESIZE,
};

struct Cursor {
    Server *server;
    wlr_cursor *cursor;
    wlr_xcursor_manager *cursor_mgr;
    wlr_cursor_shape_manager_v1 *cursor_shape_mgr;
    CursorMode cursor_mode;

    std::vector<wlr_pointer *> pointers;

    double grab_x, grab_y;
    wlr_box grab_geobox;
    uint32_t resize_edges;

    wl_listener motion;
    wl_listener motion_absolute;
    wl_listener button;
    wl_listener axis;
    wl_listener frame;

    wl_listener request_set_shape;

    wlr_pointer_constraint_v1 *active_constraint{nullptr};
    bool requires_warp{false};
    pixman_region32_t confine;
    wl_listener constraint_commit;

    Cursor(Server *server);
    ~Cursor();

    void reset_mode();
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
