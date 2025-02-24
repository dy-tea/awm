#include "Server.h"

Output::Output(Server *server, struct wlr_output *wlr_output)
    : server(server), wlr_output(wlr_output) {

    wl_list_init(&workspaces);

    // create layers
    layers.background = wlr_scene_tree_create(server->layers.background);
    layers.bottom = wlr_scene_tree_create(server->layers.bottom);
    layers.top = wlr_scene_tree_create(server->layers.top);
    layers.overlay = wlr_scene_tree_create(server->layers.overlay);

    // create workspaces
    for (int i = 0; i != 9; ++i)
        new_workspace();
    set_workspace(0);

    // point output data to this
    this->wlr_output->data = this;

    // send arrange
    server->output_manager->arrange();
    update_position();

    // frame
    frame.notify = [](wl_listener *listener, void *data) {
        // called once per frame
        Output *output = wl_container_of(listener, output, frame);
        wlr_scene *scene = output->server->scene;

        wlr_scene_output *scene_output =
            wlr_scene_get_scene_output(scene, output->wlr_output);

        // render scene
        wlr_scene_output_commit(scene_output, nullptr);

        // get frame time
        timespec now{};
        clock_gettime(CLOCK_MONOTONIC, &now);
        wlr_scene_output_send_frame_done(scene_output, &now);

        // output->arrange_layers();
    };
    wl_signal_add(&wlr_output->events.frame, &frame);

    // request_state
    request_state.notify = [](wl_listener *listener, void *data) {
        Output *output = wl_container_of(listener, output, request_state);

        const auto *event = static_cast<wlr_output_event_request_state *>(data);

        wlr_output_commit_state(output->wlr_output, event->state);
        output->arrange_layers();
    };
    wl_signal_add(&wlr_output->events.request_state, &request_state);

    // destroy
    destroy.notify = [](wl_listener *listener, void *data) {
        Output *output = wl_container_of(listener, output, destroy);
        delete output;
    };
    wl_signal_add(&wlr_output->events.destroy, &destroy);
}

Output::~Output() {
    Workspace *workspace, *tmp;
    wl_list_for_each_safe(workspace, tmp, &workspaces, link) delete workspace;

    wl_list_remove(&frame.link);
    wl_list_remove(&request_state.link);
    wl_list_remove(&destroy.link);
    wl_list_remove(&link);
}

// arrange all layers
void Output::arrange_layers() {
    wlr_box usable = {0};
    wlr_output_effective_resolution(wlr_output, &usable.width, &usable.height);
    const wlr_box full_area = usable;

    // exclusive surfaces
    arrange_layer_surface(&full_area, &usable, layers.overlay, true);
    arrange_layer_surface(&full_area, &usable, layers.top, true);
    arrange_layer_surface(&full_area, &usable, layers.bottom, true);
    arrange_layer_surface(&full_area, &usable, layers.background, true);

    // non-exclusive surfaces
    arrange_layer_surface(&full_area, &usable, layers.overlay, false);
    arrange_layer_surface(&full_area, &usable, layers.top, false);
    arrange_layer_surface(&full_area, &usable, layers.bottom, false);
    arrange_layer_surface(&full_area, &usable, layers.background, false);

    // check if usable area changed
    if (memcmp(&usable, &usable_area, sizeof(wlr_box)) != 0)
        usable_area = usable;

    // handle keyboard interactive layers
    LayerSurface *topmost = nullptr;
    wlr_scene_tree *layers_above_shell[] = {layers.overlay, layers.top};

    for (const wlr_scene_tree *layer : layers_above_shell) {
        wlr_scene_node *node;
        wl_list_for_each_reverse(node, &layer->children, link) {
            if (LayerSurface *surface = static_cast<LayerSurface *>(node->data);
                surface &&
                surface->wlr_layer_surface->current.keyboard_interactive ==
                    ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE &&
                surface->wlr_layer_surface->surface->mapped) {
                topmost = surface;
                break;
            }
        }
        if (topmost)
            break;
    }

    // TODO update keyboard focus
}

// arrange a surface layer
void Output::arrange_layer_surface(const wlr_box *full_area,
                                   wlr_box *usable_area,
                                   const wlr_scene_tree *tree,
                                   const bool exclusive) {
    wlr_scene_node *node;
    wl_list_for_each(node, &tree->children, link) {
        const LayerSurface *surface = static_cast<LayerSurface *>(node->data);
        if (!surface || !surface->wlr_layer_surface->initialized)
            continue;

        if ((surface->wlr_layer_surface->current.exclusive_zone > 0) !=
            exclusive)
            continue;

        wlr_scene_layer_surface_v1_configure(surface->scene_layer_surface,
                                             full_area, usable_area);
    }
}

// get a layer shell layer
wlr_scene_tree *
Output::shell_layer(const enum zwlr_layer_shell_v1_layer layer) const {
    switch (layer) {
    case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
        return layers.background;
    case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
        return layers.bottom;
    case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
        return layers.top;
    case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
        return layers.overlay;
    default:
        wlr_log(WLR_ERROR, "unreachable");
        return nullptr;
    }
}

// create a new workspace for this output
Workspace *Output::new_workspace() {
    Workspace *workspace = new Workspace(this, max_workspace++);

    wl_list_insert(&workspaces, &workspace->link);
    return workspace;
}

// get the workspace numbered n
Workspace *Output::get_workspace(const uint32_t n) const {
    Workspace *workspace, *tmp;
    wl_list_for_each_safe(workspace, tmp, &workspaces,
                          link) if (workspace->num == n) return workspace;

    return nullptr;
}

// get the currently focused workspace
Workspace *Output::get_active() const {
    if (wl_list_empty(&workspaces))
        return nullptr;

    Workspace *active = wl_container_of(workspaces.next, active, link);
    return active;
}

// change the focused workspace to workspace n
bool Output::set_workspace(const uint32_t n) {
    Workspace *requested = get_workspace(n);

    // workspace does not exist
    if (requested == nullptr)
        return false;

    // hide workspace we are moving from
    if (Workspace *previous = get_active())
        previous->set_hidden(true);

    // set new workspace to the active one
    wl_list_remove(&requested->link);
    wl_list_insert(&workspaces, &requested->link);

    // unhide active workspace and focus it
    requested->set_hidden(false);
    requested->focus();

    return true;
}

// update layout geometry
void Output::update_position() {
    wlr_output_layout_get_box(server->output_manager->layout, wlr_output,
                              &layout_geometry);
}

// apply a config to the output
bool Output::apply_config(const OutputConfig *config, const bool test_only) {
    // create output state
    wlr_output_state state{};
    wlr_output_state_init(&state);

    // enabled
    wlr_output_state_set_enabled(&state, config->enabled);

    if (config->enabled) {
        // set mode
        bool mode_set = false;
        if (config->width > 0 && config->height > 0 && config->refresh > 0) {
            // find matching mode
            wlr_output_mode *mode, *best_mode = nullptr;
            wl_list_for_each(
                mode, &wlr_output->modes,
                link) if ((mode->width == config->width &&
                           mode->height == config->height) &&
                          (!best_mode ||
                           (abs(static_cast<int>(mode->refresh / 1000.0 -
                                                 config->refresh)) < 1.5 &&
                            abs(static_cast<int>(mode->refresh / 1000.0 -
                                                 config->refresh)) <
                                abs(static_cast<int>(
                                    best_mode->refresh / 1000.0 -
                                    config->refresh))))) best_mode = mode;

            if (best_mode) {
                wlr_output_state_set_mode(&state, best_mode);
                mode_set = true;
            }
        }

        // set to preferred mode if not set
        if (!mode_set) {
            wlr_output_state_set_mode(&state,
                                      wlr_output_preferred_mode(wlr_output));
            wlr_log(WLR_INFO, "using fallback mode for output %s",
                    config->name.c_str());
        }

        // scale
        if (config->scale > 0)
            wlr_output_state_set_scale(&state,
                                       static_cast<float>(config->scale));

        // transform
        wlr_output_state_set_transform(&state, config->transform);

        // adaptive sync
        wlr_output_state_set_adaptive_sync_enabled(&state,
                                                   config->adaptive_sync);
    }

    bool success;
    if (test_only)
        // test and get status
        success = wlr_output_test_state(wlr_output, &state);
    else {
        // commit and get status
        success = wlr_output_commit_state(wlr_output, &state);

        if (success) {
            // update position
            wlr_output_layout_add(server->output_manager->layout, wlr_output,
                                  static_cast<int>(config->x),
                                  static_cast<int>(config->y));

            // rearrange
            arrange_layers();
        }
    }

    wlr_output_state_finish(&state);
    return success;
}
