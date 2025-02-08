#include "Server.h"

LayerShell::LayerShell(struct Server *server) {
    // create wlr_layer_shell
    this->server = server;
    wl_list_init(&layer_surfaces);

    for (int i = 0; i != 4; ++i)
        layers[i] = wlr_scene_tree_create(&server->scene->tree);

    wlr_layer_shell = wlr_layer_shell_v1_create(server->wl_display, 5);

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
            Output *output = shell->server->get_output(0);

            if (!output) {
                wlr_log(WLR_ERROR, "no available output for layer surface");
                return;
            }

            shell_surface->output = output->wlr_output;
        }

        // add surface to list so it can be freed later
        LayerSurface *layer_surface = new LayerSurface(shell, shell_surface);
        if (layer_surface)
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

void LayerShell::arrange_layers(struct Output *output) {
    if (!output) {
        wlr_log(WLR_ERROR, "Attempted to arrange layers with null output");
        return;
    }
    if (!output->wlr_output) {
        wlr_log(WLR_ERROR, "Output has null wlr_output");
        return;
    }
    if (!output->wlr_output->data) {
        wlr_log(WLR_ERROR, "wlr_output has null data field");
        return;
    }
    if (output->wlr_output->data != output) {
        wlr_log(WLR_ERROR,
                "wlr_output data field doesn't match Output pointer");
        return;
    }

    if (!output || !output->wlr_output)
        return;

    struct wlr_box usable_area = {0};
    wlr_output_effective_resolution(output->wlr_output, &usable_area.width,
                                    &usable_area.height);
    struct wlr_box full_area = usable_area;

    // arrange exclusive surfaces
    for (int i = 0; i != 4; ++i) {
        struct wlr_scene_node *node;
        wl_list_for_each(node, &layers[i]->children, link) {
            LayerSurface *surface = (LayerSurface *)node->data;
            if (!surface || !surface->wlr_layer_surface->initialized)
                continue;

            if (surface->wlr_layer_surface->current.exclusive_zone > 0)
                wlr_scene_layer_surface_v1_configure(
                    surface->scene_layer_surface, &full_area, &usable_area);
        }
    }

    // arrange non-exclusive surfaces
    for (int i = 0; i != 4; ++i) {
        struct wlr_scene_node *node;
        wl_list_for_each(node, &layers[i]->children, link) {
            LayerSurface *surface = (LayerSurface *)node->data;
            if (!surface || !surface->wlr_layer_surface->initialized)
                continue;

            if (surface->wlr_layer_surface->current.exclusive_zone <= 0)
                wlr_scene_layer_surface_v1_configure(
                    surface->scene_layer_surface, &full_area, &usable_area);
        }
    }

    // output->usable_area = usable_area;
}

struct wlr_scene_tree *
LayerShell::get_layer_scene(enum zwlr_layer_shell_v1_layer type) {
    switch (type) {
    case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
        return layers[0];
    case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
        return layers[1];
    case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
        return layers[2];
    case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
        return layers[3];
    default:
        return layers[0];
    };
}
