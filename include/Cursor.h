#include "wlr.h"
#include <vector>

enum CursorMode {
    CURSORMODE_PASSTHROUGH,
    CURSORMODE_MOVE,
    CURSORMODE_RESIZE,
};

struct Cursor {
    struct Server *server;
    struct wlr_cursor *cursor;
    struct wlr_xcursor_manager *cursor_mgr;
    struct wlr_cursor_shape_manager_v1 *cursor_shape_mgr;
    enum CursorMode cursor_mode;

    std::vector<wlr_pointer *> pointers;

    double grab_x, grab_y;
    struct wlr_box grab_geobox;
    uint32_t resize_edges;

    struct wl_listener motion;
    struct wl_listener motion_absolute;
    struct wl_listener button;
    struct wl_listener axis;
    struct wl_listener frame;

    struct wl_listener request_set_shape;

    Cursor(struct Server *server);
    ~Cursor();

    void reset_mode();
    void process_motion(uint32_t time, struct wlr_input_device *device,
                        double dx, double dy, double unaccel_dx,
                        double unaccel_dy);
    void process_move();
    void process_resize();

    void set_config(struct wlr_pointer *pointer);
    void reconfigure_all();
};
