#include "Server.h"

LayerShell::LayerShell(struct wl_display *wl_display, struct wlr_scene *scene) {
    this->scene = scene;
    wl_list_init(&layer_surfaces);
    wlr_layer_shell = wlr_layer_shell_v1_create(wl_display, 4);

    if (!wlr_layer_shell) {
        wlr_log(WLR_ERROR, "Failed to create wlr_layer_shell_v1");
        return;
    }

    // new_surface
    new_shell_surface.notify = [](struct wl_listener *listener, void *data) {
        LayerShell *shell = wl_container_of(listener, shell, new_shell_surface);
        wlr_layer_surface_v1 *shell_surface =
            static_cast<wlr_layer_surface_v1 *>(data);

        if (!shell_surface->output) {
            Server *server = wl_container_of(listener, server, outputs);
            Output *output = server->get_output(0);
            shell_surface->output = output->wlr_output;
        }

        LayerSurface *layer_surface = new LayerSurface(shell, shell_surface);
    };
    wl_signal_add(&wlr_layer_shell->events.new_surface, &new_shell_surface);

    // destroy
    destroy.notify = [](struct wl_listener *listener, void *data) {
        LayerShell *shell = wl_container_of(listener, shell, destroy);
        delete shell;
    };
    wl_signal_add(&wlr_layer_shell->events.destroy, &destroy);
}

LayerShell::~LayerShell() {
    wl_list_remove(&new_shell_surface.link);
    wl_list_remove(&destroy.link);

    struct LayerSurface *surface, *tmp;
    wl_list_for_each_safe(surface, tmp, &layer_surfaces, link)
        delete surface;
}
