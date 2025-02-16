#include <cassert>
#include <sys/wait.h>
#include <unistd.h>

#include "Keyboard.h"
#include "LayerSurface.h"
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

    struct wlr_linux_dmabuf_v1 *wlr_linux_dmabuf;

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
    struct wl_listener update_monitors;

    struct wl_list outputs;
    struct wl_listener new_output;

    struct {
        struct wlr_scene_tree *background;
        struct wlr_scene_tree *bottom;
        struct wlr_scene_tree *floating;
        struct wlr_scene_tree *fullscreen;
        struct wlr_scene_tree *top;
        struct wlr_scene_tree *overlay;
    } layers;

    // struct XWaylandShell *xwayland_shell;
    struct wl_list layer_surfaces;
    struct wlr_layer_shell_v1 *wlr_layer_shell;
    struct wl_listener new_shell_surface;

    struct wlr_xdg_output_manager_v1 *wlr_xdg_output_manager;
    struct wlr_output_manager_v1 *wlr_output_manager;
    struct wl_listener output_apply;
    struct wl_listener output_test;

    struct wlr_virtual_pointer_manager_v1 *virtual_pointer_mgr;
    struct wl_listener new_virtual_pointer;



    struct wlr_export_dmabuf_manager_v1 *wlr_export_dmabuf_manager;
    struct wlr_screencopy_manager_v1 *wlr_screencopy_manager;
    struct wlr_ext_foreign_toplevel_list_v1 *wlr_foreign_toplevel_list;
    struct wlr_foreign_toplevel_manager_v1 *wlr_foreign_toplevel_manager;
    struct wlr_data_control_manager_v1 *wlr_data_control_manager;
    struct wlr_gamma_control_manager_v1 *wlr_gamma_control_manager;
    struct wlr_ext_image_copy_capture_manager_v1
        *ext_image_copy_capture_manager;
    struct wlr_fractional_scale_manager_v1 *wlr_fractional_scale_manager;

    Server(struct Config *config);
    ~Server();

    void new_keyboard(struct wlr_input_device *device);
    void new_pointer(struct wlr_input_device *device);

    struct Output *get_output(struct wlr_output *wlr_output);
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

    struct Workspace *get_workspace(struct Toplevel *toplevel);

    bool apply_output_config_to_output(Output *output, OutputConfig *config,
                                       bool test_only);
    void apply_output_config(struct wlr_output_configuration_v1 *config,
                             bool test_only);

    void arrange();
};
