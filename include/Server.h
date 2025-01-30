#include <cassert>
#include <unistd.h>

#include "Keyboard.h"
#include "LayerShell.h"
#include "Output.h"
#include "Popup.h"
#include "Toplevel.h"
#include "XWaylandShell.h"

struct Server {
    struct wl_display *wl_display;
    struct wlr_backend *backend;
    struct wlr_renderer *renderer;
    struct wlr_allocator *allocator;
    struct wlr_compositor *compositor;
    struct wlr_scene *scene;
    struct wlr_scene_output_layout *scene_layout;

    struct wlr_xdg_shell *xdg_shell;
    struct wl_listener new_xdg_toplevel;
    struct wl_listener new_xdg_popup;
    struct wl_list toplevels;

    struct wl_listener renderer_lost;

    struct wlr_cursor *cursor;
    struct wlr_xcursor_manager *cursor_mgr;
    struct wl_listener cursor_motion;
    struct wl_listener cursor_motion_absolute;
    struct wl_listener cursor_button;
    struct wl_listener cursor_axis;
    struct wl_listener cursor_frame;

    struct wlr_seat *seat;
    struct wl_listener new_input;
    struct wl_listener request_cursor;
    struct wl_listener request_set_selection;
    struct wl_list keyboards;
    enum CursorMode cursor_mode;
    struct Toplevel *grabbed_toplevel;
    double grab_x, grab_y;
    struct wlr_box grab_geobox;
    uint32_t resize_edges;

    struct wlr_output_layout *output_layout;
    struct wl_list outputs;
    struct wl_listener new_output;

    // struct XWaylandShell *xwayland_shell;
    struct LayerShell *layer_shell;

    Server(const char *startup_cmd);
    ~Server();

    void new_keyboard(struct wlr_input_device *device);
    void new_pointer(struct wlr_input_device *device);

    void reset_cursor_mode();
    void process_cursor_move();
    void process_cursor_resize();
    void process_cursor_motion(uint32_t time);

    struct Output *get_output(uint32_t index);

    template <typename T>
    T *surface_at(double lx, double ly, struct wlr_surface **surface,
                  double *sx, double *sy);
    struct Toplevel *toplevel_at(double lx, double ly,
                                 struct wlr_surface **surface, double *sx,
                                 double *sy);
    struct LayerSurface *layer_surface_at(double lx, double ly,
                                          struct wlr_surface **surface,
                                          double *sx, double *sy);
};
