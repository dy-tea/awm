#include "Output.h"
#include "Config.h"
#include "LayerSurface.h"
#include "OutputManager.h"
#include "Server.h"
#include "WorkspaceManager.h"

Output::Output(Server *server, struct wlr_output *wlr_output)
    : server(server), wlr_output(wlr_output) {
    // set allocator and renderer for output
    wlr_output_init_render(wlr_output, server->allocator, server->renderer);

    // add to outputs list
    OutputManager *manager = server->output_manager;
    wl_list_insert(&manager->outputs, &link);

    // create initial workspaces through WorkspaceManager
    if (!server->workspace_manager->adopt_workspaces(this)) {
        for (int i = 1; i <= 10; ++i)
            server->workspace_manager->new_workspace(this, i);
        set_workspace(1);
    }

    // create layers
    layers.background = wlr_scene_tree_create(server->layers.background);
    layers.bottom = wlr_scene_tree_create(server->layers.bottom);
    layers.top = wlr_scene_tree_create(server->layers.top);
    layers.overlay = wlr_scene_tree_create(server->layers.overlay);

    // send arrange
    manager->arrange();

    // point output data to this
    wlr_output->data = this;

    // frame
    frame.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
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
    };
    wl_signal_add(&wlr_output->events.frame, &frame);

    // request_state
    request_state.notify = [](wl_listener *listener, void *data) {
        Output *output = wl_container_of(listener, output, request_state);

        const auto *event = static_cast<wlr_output_event_request_state *>(data);

        wlr_output_commit_state(output->wlr_output, event->state);
        output->arrange_layers();
        output->update_position();
    };
    wl_signal_add(&wlr_output->events.request_state, &request_state);

    // destroy
    destroy.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        Output *output = wl_container_of(listener, output, destroy);
        delete output;
    };
    wl_signal_add(&wlr_output->events.destroy, &destroy);

    // find matching config
    OutputConfig *matching = nullptr;
    for (OutputConfig *config : server->config->outputs) {
        if (config->name == wlr_output->name) {
            matching = config;
            break;
        }
    }

    // apply the config
    bool config_success = matching && apply_config(matching, false);

    // fallback config
    if (!config_success) {
        wlr_log(WLR_INFO, "using fallback mode for output %s",
                wlr_output->name);

        // create new state
        wlr_output_state state{};
        wlr_output_state_init(&state);

        // use preferred mode
        wlr_output_state_set_enabled(&state, true);
        wlr_output_state_set_mode(&state,
                                  wlr_output_preferred_mode(wlr_output));

        // commit state
        config_success = wlr_output_commit_state(wlr_output, &state);
        wlr_output_state_finish(&state);
    }

    // position
    wlr_output_layout_output *output_layout_output =
        matching && config_success
            ? wlr_output_layout_add(manager->layout, wlr_output, matching->x,
                                    matching->y)
            : wlr_output_layout_add_auto(manager->layout, wlr_output);

    // add to scene output
    wlr_scene_output *scene_output =
        wlr_scene_output_create(server->scene, wlr_output);
    wlr_scene_output_layout_add_output(server->scene_layout,
                                       output_layout_output, scene_output);

    // set geometry
    wlr_output_layout_get_box(manager->layout, wlr_output, &layout_geometry);
    memcpy(&usable_area, &layout_geometry, sizeof(wlr_box));
}

Output::~Output() {
    server->workspace_manager->orphanize_workspaces(this);

    wl_list_remove(&frame.link);
    wl_list_remove(&request_state.link);
    wl_list_remove(&destroy.link);
    wl_list_remove(&link);
}

// arrange all layers
void Output::arrange_layers() {
    // output must be enabled to have an effective resolution
    if (!enabled)
        return;

    wlr_box usable = {};
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
            LayerSurface *surface = static_cast<LayerSurface *>(node->data);
            if (server->locked || !surface ||
                surface->wlr_layer_surface->current.keyboard_interactive !=
                    ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE ||
                !surface->wlr_layer_surface->surface->mapped)
                continue;

            topmost = surface;
            break;
        }
        if (topmost)
            break;
    }
}

// arrange a surface layer
void Output::arrange_layer_surface(const wlr_box *full, wlr_box *usable,
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

        wlr_scene_layer_surface_v1_configure(surface->scene_layer_surface, full,
                                             usable);
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
        throw std::runtime_error("unreachable: invalid layer");
        return nullptr;
    }
}

// create a new workspace for this output
Workspace *Output::new_workspace() {
    return server->workspace_manager->new_workspace(this);
}

Workspace *Output::get_active() {
    return server->workspace_manager->get_active_workspace(this);
}

Workspace *Output::get_workspace(const uint32_t n) {
    return server->workspace_manager->get_workspace(n, this);
}

// change the focused workspace to passed workspace
bool Output::set_workspace(Workspace *workspace) {
    return server->workspace_manager->set_workspace(workspace);
}

// change the focused workspace to workspace n
bool Output::set_workspace(const uint32_t n) {
    return server->workspace_manager->set_workspace(n, this);
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
            wl_list_for_each(mode, &wlr_output->modes, link)
                // find mode where width and height are equal to OutputConfig
                // and refresh rate is within 1.5
                if ((mode->width == config->width &&
                     mode->height == config->height) &&
                    (!best_mode ||
                     (abs(static_cast<int>(mode->refresh / 1000.0 -
                                           config->refresh)) < 1.5 &&
                      abs(static_cast<int>(mode->refresh / 1000.0 -
                                           config->refresh)) <
                          abs(static_cast<int>(best_mode->refresh / 1000.0 -
                                               config->refresh))))) best_mode =
                    mode;

            // set mode if found
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
        if (config->scale > 0.0)
            wlr_output_state_set_scale(&state, config->scale);

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
