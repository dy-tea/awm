#include <algorithm>
#include <cassert>
#include <sys/wait.h>
#include <unistd.h>

#include "Keyboard.h"
#include "LayerSurface.h"
#include "Output.h"
#include "OutputManager.h"
#include "Popup.h"
#include "Toplevel.h"
#include "Workspace.h"
#include "wlr.h"

struct Server {
    // get singleton instance
    static Server &get() {
        static Server server;
        return server;
    };

    Config *config;

    wl_display *display;
    wlr_backend *backend;
    wlr_renderer *renderer;
    wlr_allocator *allocator;
    wlr_compositor *compositor;
    wlr_scene *scene;
    wlr_scene_output_layout *scene_layout;

    wlr_linux_dmabuf_v1 *wlr_linux_dmabuf;

    wl_listener renderer_lost;

    wlr_xdg_shell *xdg_shell;
    wl_listener new_xdg_toplevel;
    wl_listener new_xdg_popup;

    wlr_relative_pointer_manager_v1 *wlr_relative_pointer_manager;

    Cursor *cursor;

    wlr_seat *seat;
    wl_listener new_input;
    wl_listener request_cursor;
    wl_listener request_set_selection;

    wl_list keyboards;

    Toplevel *grabbed_toplevel;

    OutputManager *output_manager;

    struct {
        wlr_scene_tree *background;
        wlr_scene_tree *bottom;
        wlr_scene_tree *floating;
        wlr_scene_tree *fullscreen;
        wlr_scene_tree *top;
        wlr_scene_tree *overlay;
    } layers;

    wl_list layer_surfaces;
    wlr_layer_shell_v1 *wlr_layer_shell;
    wl_listener new_shell_surface;

    wlr_virtual_pointer_manager_v1 *virtual_pointer_mgr;
    wl_listener new_virtual_pointer;

    struct wlr_viewporter *wlr_viewporter;
    struct wlr_presentation *wlr_presentation;
    wlr_export_dmabuf_manager_v1 *wlr_export_dmabuf_manager;
    wlr_screencopy_manager_v1 *wlr_screencopy_manager;
    wlr_ext_foreign_toplevel_list_v1 *wlr_foreign_toplevel_list;
    wlr_foreign_toplevel_manager_v1 *wlr_foreign_toplevel_manager;
    wlr_data_control_manager_v1 *wlr_data_control_manager;
    wlr_gamma_control_manager_v1 *wlr_gamma_control_manager;
    wlr_ext_image_copy_capture_manager_v1 *ext_image_copy_capture_manager;
    wlr_fractional_scale_manager_v1 *wlr_fractional_scale_manager;
    wlr_alpha_modifier_v1 *wlr_alpha_modifier;
    wlr_single_pixel_buffer_manager_v1 *wlr_single_pixel_buffer_manager;

#ifdef XWAYLAND
    wlr_xwayland *xwayland;
    wl_listener xwayland_ready;
    wl_listener new_xwayland_surface;
#endif

    Server() = default;
    Server(Config *config);
    ~Server();

    void exit() const;

    Output *get_output(const wlr_output *wlr_output) const;
    Output *focused_output() const;

    template <typename T>
    T *surface_at(double lx, double ly, wlr_surface **surface, double *sx,
                  double *sy);
    Toplevel *toplevel_at(double lx, double ly, wlr_surface **surface,
                          double *sx, double *sy);
    LayerSurface *layer_surface_at(double lx, double ly, wlr_surface **surface,
                                   double *sx, double *sy);

    Workspace *get_workspace(Toplevel *toplevel) const;
};
