#include "LayerShell.h"

LayerShell::LayerShell(struct wl_display *wl_display) {
  wlr_layer_shell = wlr_layer_shell_v1_create(wl_display, 1);
  wlr_layer_shell->data = this;

  // new_surface
  new_shell_surface.notify = [](struct wl_listener *listener, void *data) {
    LayerShell *shell = wl_container_of(listener, shell, new_shell_surface);
    wlr_layer_surface_v1 *shell_surface =
        static_cast<wlr_layer_surface_v1 *>(data);

    LayerSurface *layer_surface = new LayerSurface(shell_surface);

    shell->pending.emplace_back(layer_surface);
    wlr_log(WLR_ERROR, "New shell surface created");
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
}
