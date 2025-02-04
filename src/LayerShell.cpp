#include "Server.h"

LayerShell::LayerShell(struct wl_display *wl_display, struct wlr_scene *scene,
                       struct wlr_seat *seat) {
    // create wlr_layer_shell
    this->scene = scene;
    this->seat = seat;
    wl_list_init(&layer_surfaces);
    wlr_layer_shell = wlr_layer_shell_v1_create(wl_display, 5);

    if (!wlr_layer_shell) {
        wlr_log(WLR_ERROR, "Failed to create wlr_layer_shell_v1");
        return;
    }

    // new_surface
    new_shell_surface.notify = [](struct wl_listener *listener, void *data) {
        // layer surface created
        LayerShell *shell = wl_container_of(listener, shell, new_shell_surface);
        wlr_layer_surface_v1 *shell_surface =
            static_cast<wlr_layer_surface_v1 *>(data);

        // assume output 0 if not set
        if (!shell_surface->output) {
            Server *server = wl_container_of(listener, server, outputs);
            Output *output = server->get_output(0);
            shell_surface->output = output->wlr_output;
        }

        // add surface to list so it can be freed later
        LayerSurface *layer_surface = new LayerSurface(shell, shell_surface);
        wl_list_insert(&shell->layer_surfaces, &layer_surface->link);
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
    wl_list_for_each_safe(surface, tmp, &layer_surfaces, link) delete surface;
}
