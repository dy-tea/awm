#include <cassert>
#include <unistd.h>

#include "Keyboard.h"
#include "LayerShell.h"
#include "Output.h"
#include "Popup.h"
#include "Toplevel.h"
#include "Workspace.h"
#include "XWaylandShell.h"

struct Server {
    struct Config *config;

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

    struct wl_listener renderer_lost;

    struct Cursor *cursor;

    struct wlr_seat *seat;
    struct wl_listener new_input;
    struct wl_listener request_cursor;
    struct wl_listener request_set_selection;

    struct wl_list keyboards;

    struct Toplevel *grabbed_toplevel;

    struct wlr_output_layout *output_layout;
    struct wl_list outputs;
    struct wl_listener new_output;

    // struct XWaylandShell *xwayland_shell;
    struct LayerShell *layer_shell;

    struct wlr_xdg_output_manager_v1 *wlr_xdg_output_manager;
    struct wlr_screencopy_manager_v1 *wlr_screencopy_manager;
    struct wlr_foreign_toplevel_manager_v1 *wlr_foreign_toplevel_manager;
    struct wlr_data_control_manager_v1 *wlr_data_control_manager;
    struct wlr_gamma_control_manager_v1 *wlr_gamma_control_manager;

    Server(struct Config *config);
    ~Server();

    void new_keyboard(struct wlr_input_device *device);
    void new_pointer(struct wlr_input_device *device);

    struct Output *get_output(uint32_t index);
    struct Output *focused_output();

    template <typename T>
    T *surface_at(double lx, double ly, struct wlr_surface **surface,
                  double *sx, double *sy);
    struct Toplevel *toplevel_at(double lx, double ly,
                                 struct wlr_surface **surface, double *sx,
                                 double *sy);
    struct LayerSurface *layer_surface_at(double lx, double ly,
                                          struct wlr_surface **surface,
                                          double *sx, double *sy);
    struct Output *output_at(double x, double y);
};
