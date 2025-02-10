#include "Server.h"

// create a new keyboard
void Server::new_keyboard(struct wlr_input_device *device) {
    struct Keyboard *keyboard = new Keyboard(this, device);

    wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);

    wl_list_insert(&keyboards, &keyboard->link);
}

// create a new pointer
void Server::new_pointer(struct wlr_input_device *device) {
    /* We don't do anything special with pointers. All of our pointer handling
     * is proxied through wlr_cursor. On another compositor, you might take this
     * opportunity to do libinput configuration on the device to set
     * acceleration, etc. */
    wlr_cursor_attach_input_device(cursor->cursor, device);
}

// get a node tree surface from its location and cast it to the generic
// type provided
template <typename T>
T *Server::surface_at(double lx, double ly, struct wlr_surface **surface,
                      double *sx, double *sy) {
    // get the scene node and ensure it's a buffer
    struct wlr_scene_node *node =
        wlr_scene_node_at(&scene->tree.node, lx, ly, sx, sy);
    if (!node || node->type != WLR_SCENE_NODE_BUFFER)
        return NULL;

    // get the scene buffer and surface of the node
    struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
    struct wlr_scene_surface *scene_surface =
        wlr_scene_surface_try_from_buffer(scene_buffer);
    if (!scene_surface || !scene_surface->surface)
        return NULL;

    // set the scene surface
    *surface = scene_surface->surface;

    // get the scene tree of the node's parent
    struct wlr_scene_tree *tree = node->parent;
    if (!tree || tree->node.type != WLR_SCENE_NODE_TREE)
        return NULL;

    // find the topmost node of the scene tree
    while (tree && tree->node.data == NULL)
        tree = tree->node.parent;

    // invalid tree
    if (!tree || !tree->node.parent)
        return NULL;

    // return the topmost node's data
    return static_cast<T *>(tree->node.data);
}

// find a toplevel by location
struct Toplevel *Server::toplevel_at(double lx, double ly,
                                     struct wlr_surface **surface, double *sx,
                                     double *sy) {
    return surface_at<Toplevel>(lx, ly, surface, sx, sy);
}

// find a layer surface by location
struct LayerSurface *Server::layer_surface_at(double lx, double ly,
                                              struct wlr_surface **surface,
                                              double *sx, double *sy) {
    return surface_at<LayerSurface>(lx, ly, surface, sx, sy);
}

// get the nth output
struct Output *Server::get_output(uint32_t index) {
    if (wl_list_empty(&outputs))
        return nullptr;

    Output *output, *tmp;
    uint32_t i = 0;

    wl_list_for_each_safe(output, tmp, &outputs, link) {
        if (i == index)
            return output;
        ++i;
    }

    return nullptr;
}

// get the output based on screen coordinates
struct Output *Server::output_at(double x, double y) {
    struct wlr_output *wlr_output =
        wlr_output_layout_output_at(output_layout, x, y);

    if (wlr_output == NULL)
        return NULL;

    struct Output *output;
    wl_list_for_each(output, &outputs,
                     link) if (output->wlr_output == wlr_output) return output;

    return NULL;
}

// get the focused output
struct Output *Server::focused_output() {
    return output_at(cursor->cursor->x, cursor->cursor->y);
}

// attempt to apply a config passed from a client to an output
void Server::apply_output_config(struct wlr_output_configuration_v1 *cfg,
                                 bool test_only) {
    // copy current output config to configs
    std::vector<OutputConfig *> configs;
    for (OutputConfig *c : config->outputs)
        configs.push_back(c);

    // add the passed cfg heads to configs
    struct wlr_output_configuration_head_v1 *config_head;
    wl_list_for_each(config_head, &cfg->heads, link) {
        struct OutputConfig *oc = new OutputConfig(config_head);
        configs.emplace_back(oc);
    }

    // match configs to output
    std::vector<MatchOutputConfig *> matches;
    Output *output, *tmp;
    wl_list_for_each_safe(output, tmp, &outputs, link) {
        struct MatchOutputConfig *match = new MatchOutputConfig;
        match->output = output;
        for (OutputConfig *oc : configs)
            if (oc->name == output->wlr_output->name) {
                match->config = oc;
                break;
            }
        matches.push_back(match);
    }

    // apply config to each output
    bool success = true;
    for (MatchOutputConfig *moc : matches) {
        if (!moc->config)
            continue;

        struct wlr_output *wlr_output = moc->output->wlr_output;
        OutputConfig *config = moc->config;

        // create new state
        struct wlr_output_state state;
        wlr_output_state_init(&state);

        // enabled
        wlr_output_state_set_enabled(&state, config->enabled);

        if (config->enabled) {
            // width / height / refresh
            if (config->width > 0 && config->height > 0 &&
                config->refresh > 0) {
                // try to find a matching mode
                struct wlr_output_mode *mode;
                bool found = false;
                wl_list_for_each(mode, &wlr_output->modes,
                                 link) if (mode->width == config->width &&
                                           mode->height == config->height &&
                                           abs((int)(mode->refresh / 1000.0 -
                                                     config->refresh)) < 1) {
                    wlr_output_state_set_mode(&state, mode);
                    found = true;
                    wlr_log(WLR_INFO, "found matching output mode: %dx%d@%.1f", mode->width, mode->height, mode->refresh / 1000.0);
                    break;
                }

                // no matching mode, try custom one
                if (!found) {
                    wlr_output_state_set_custom_mode(
                        &state, config->width, config->height,
                        (int)(config->refresh * 1000));
                    wlr_log(WLR_INFO, "attempting to set custom mode: %dx%d@%.1f", config->width, config->height, config->refresh);
                }
            }

            // position
            wlr_output_layout_output_coords(output_layout, wlr_output,
                                            &config->x, &config->y);

            // scale
            if (config->scale > 0)
                wlr_output_state_set_scale(&state, config->scale);

            // transform
            wlr_output_state_set_transform(&state, config->transform);

            // adaptive sync
            wlr_output_state_set_adaptive_sync_enabled(&state,
                                                       config->adaptive_sync);
        }

        // test or apply
        if (test_only)
            success &= wlr_output_test_state(wlr_output, &state);
        else
            success &= wlr_output_commit_state(wlr_output, &state);

        wlr_output_state_finish(&state);
    }

    // dealloc
    for (MatchOutputConfig *moc : matches)
        delete moc;

    // send cfg status
    if (success)
        wlr_output_configuration_v1_send_succeeded(cfg);
    else
        wlr_output_configuration_v1_send_failed(cfg);
}

Server::Server(struct Config *config) {
    // set config from file
    this->config = config;

    // display
    wl_display = wl_display_create();

    // backend
    backend =
        wlr_backend_autocreate(wl_display_get_event_loop(wl_display), NULL);
    if (backend == NULL) {
        wlr_log(WLR_ERROR, "failed to create wlr_backend");
        exit(1);
    }

    // renderer
    renderer = wlr_renderer_autocreate(backend);
    if (renderer == NULL) {
        wlr_log(WLR_ERROR, "failed to create wlr_renderer");
        exit(1);
    }

    wlr_renderer_init_wl_display(renderer, wl_display);

    // render allocator
    allocator = wlr_allocator_autocreate(backend, renderer);
    if (allocator == NULL) {
        wlr_log(WLR_ERROR, "failed to create wlr_allocator");
        exit(1);
    }

    // wlr compositor
    compositor = wlr_compositor_create(wl_display, 5, renderer);
    wlr_subcompositor_create(wl_display);
    wlr_data_device_manager_create(wl_display);

    // outputs
    output_layout = wlr_output_layout_create(wl_display);
    wl_list_init(&outputs);

    // new_output FIXME holy code duplication (apply_output_configuration)
    new_output.notify = [](struct wl_listener *listener, void *data) {
        // new display / monitor available
        struct Server *server = wl_container_of(listener, server, new_output);
        struct wlr_output *wlr_output = static_cast<struct wlr_output *>(data);

        // set allocator and renderer for output
        wlr_output_init_render(wlr_output, server->allocator, server->renderer);

        // find matching config
        OutputConfig *matching_config = nullptr;
        for (OutputConfig *config : server->config->outputs) {
            if (config->name == wlr_output->name) {
                matching_config = config;
                break;
            }
        }

        // create new state
        struct wlr_output_state state;
        wlr_output_state_init(&state);

        // try with saved configuration
        bool config_success = false;
        if (matching_config) {
            wlr_log(WLR_INFO, "attempting to apply saved configuration for %s",
                    wlr_output->name);

            // enable
            wlr_output_state_set_enabled(&state, matching_config->enabled);

            if (matching_config->enabled) {
                if (matching_config->width > 0 && matching_config->height > 0 &&
                    matching_config->refresh > 0) {
                    // try to find a matching mode
                    struct wlr_output_mode *mode, *best_mode = nullptr;
                    wl_list_for_each(mode, &wlr_output->modes, link) if (
                        mode->width == matching_config->width &&
                        mode->height ==
                            matching_config
                                ->height) if (!best_mode ||
                                              abs((int)(mode->refresh / 1000.0 -
                                                        matching_config
                                                            ->refresh)) <
                                                  abs((int)(best_mode->refresh /
                                                                1000.0 -
                                                            matching_config
                                                                ->refresh)))
                                                                    best_mode = mode;

                    // set mode if found
                    if (best_mode) {
                        wlr_output_state_set_mode(&state, best_mode);
                        wlr_log(WLR_INFO, "found matching output mode: %dx%d@%.1f", best_mode->width, best_mode->height, best_mode->refresh / 1000.0);
                    }
                }

                // scale
                if (matching_config->scale > 0)
                    wlr_output_state_set_scale(&state, matching_config->scale);

                // transform
                wlr_output_state_set_transform(&state,
                                               matching_config->transform);
            }

            // try to commit the configuration
            config_success = wlr_output_test_state(wlr_output, &state);
        }

        // try default config
        if (!config_success) {
            wlr_log(WLR_INFO, "falling back to default configuration for %s",
                    wlr_output->name);

            wlr_output_state_finish(&state);
            wlr_output_state_init(&state);

            wlr_output_state_set_enabled(&state, true);

            // set preferred mode
            struct wlr_output_mode *mode =
                wlr_output_preferred_mode(wlr_output);
            if (mode != NULL)
                wlr_output_state_set_mode(&state, mode);
        }

        // commit final config
        if (!wlr_output_commit_state(wlr_output, &state))
            wlr_log(WLR_ERROR, "failed to commit output state for %s",
                    wlr_output->name);

        wlr_output_state_finish(&state);

        // add to outputs list
        struct Output *output = new Output(server, wlr_output);
        wl_list_insert(&server->outputs, &output->link);

        // position
        if (matching_config && config_success)
            wlr_output_layout_add(server->output_layout, wlr_output,
                                  matching_config->x, matching_config->y);
        else
            wlr_output_layout_add_auto(server->output_layout, wlr_output);
    };
    wl_signal_add(&backend->events.new_output, &new_output);

    // scene
    scene = wlr_scene_create();
    scene_layout = wlr_scene_attach_output_layout(scene, output_layout);

    // layer shell
    layer_shell = new LayerShell(this);

    // create scene tree for toplevels between layer shell bottom and top layers
    toplevel_tree = wlr_scene_tree_create(&scene->tree);
    wlr_scene_node_place_below(
        &toplevel_tree->node,
        &layer_shell->get_layer_scene(ZWLR_LAYER_SHELL_V1_LAYER_TOP)->node);
    wlr_scene_node_place_above(
        &toplevel_tree->node,
        &layer_shell->get_layer_scene(ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM)->node);

    // create scene tree for fullscreened toplevels between layer shell top and
    // overlay layers
    fullscreen_tree = wlr_scene_tree_create(&scene->tree);
    wlr_scene_node_place_below(
        &fullscreen_tree->node,
        &layer_shell->get_layer_scene(ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY)->node);
    wlr_scene_node_place_above(
        &fullscreen_tree->node,
        &layer_shell->get_layer_scene(ZWLR_LAYER_SHELL_V1_LAYER_TOP)->node);

    // create xdg shell
    xdg_shell = wlr_xdg_shell_create(wl_display, 6);

    // renderer_lost
    renderer_lost.notify = [](struct wl_listener *listener, void *data) {
        // renderer recovery (thanks sway)
        struct Server *server =
            wl_container_of(listener, server, renderer_lost);

        wlr_log(WLR_INFO, "Re-creating renderer after GPU reset");

        // create new renderer
        struct wlr_renderer *renderer =
            wlr_renderer_autocreate(server->backend);
        if (renderer == NULL) {
            wlr_log(WLR_ERROR, "Unable to create renderer");
            return;
        }

        // create new allocator
        struct wlr_allocator *allocator =
            wlr_allocator_autocreate(server->backend, renderer);
        if (allocator == NULL) {
            wlr_log(WLR_ERROR, "Unable to create allocator");
            wlr_renderer_destroy(renderer);
            return;
        }

        // replace old and renderer and allocator
        struct wlr_renderer *old_renderer = server->renderer;
        struct wlr_allocator *old_allocator = server->allocator;
        server->renderer = renderer;
        server->allocator = allocator;

        // reset signal
        wl_list_remove(&server->renderer_lost.link);
        wl_signal_add(&server->renderer->events.lost, &server->renderer_lost);

        // move compositor to new renderer
        wlr_compositor_set_renderer(server->compositor, renderer);

        // reinint outputs
        struct Output *output, *tmp;
        wl_list_for_each_safe(output, tmp, &server->outputs, link)
            wlr_output_init_render(output->wlr_output, server->allocator,
                                   server->renderer);

        // destroy old renderer and allocator
        wlr_allocator_destroy(old_allocator);
        wlr_renderer_destroy(old_renderer);
    };
    wl_signal_add(&renderer->events.lost, &renderer_lost);

    // new_xdg_toplevel
    new_xdg_toplevel.notify = [](struct wl_listener *listener, void *data) {
        struct Server *server =
            wl_container_of(listener, server, new_xdg_toplevel);

        // toplevels are managed by workspaces
        [[maybe_unused]] struct Toplevel *toplevel =
            new Toplevel(server, (wlr_xdg_toplevel *)data);
    };
    wl_signal_add(&xdg_shell->events.new_toplevel, &new_xdg_toplevel);

    // new_xdg_popup
    new_xdg_popup.notify = [](struct wl_listener *listener, void *data) {
        struct wlr_xdg_popup *xdg_popup = (wlr_xdg_popup *)data;

        // popups do not need to be tracked
        [[maybe_unused]] struct Popup *popup = new Popup(xdg_popup);
    };
    wl_signal_add(&xdg_shell->events.new_popup, &new_xdg_popup);

    // cursor
    cursor = new Cursor(this);

    // keyboards
    wl_list_init(&keyboards);

    // new_input
    new_input.notify = [](struct wl_listener *listener, void *data) {
        // create input device based on type
        struct Server *server = wl_container_of(listener, server, new_input);
        struct wlr_input_device *device = (wlr_input_device *)data;

        switch (device->type) {
        case WLR_INPUT_DEVICE_KEYBOARD:
            server->new_keyboard(device);
            break;
        case WLR_INPUT_DEVICE_POINTER:
            server->new_pointer(device);
            break;
        default:
            break;
        }

        // set input device capabilities
        uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
        if (!wl_list_empty(&server->keyboards))
            caps |= WL_SEAT_CAPABILITY_KEYBOARD;

        wlr_seat_set_capabilities(server->seat, caps);
    };
    wl_signal_add(&backend->events.new_input, &new_input);

    // seat
    seat = wlr_seat_create(wl_display, "seat0");

    // request_cursor (seat)
    request_cursor.notify = [](struct wl_listener *listener, void *data) {
        // client-provided cursor image
        struct Server *server =
            wl_container_of(listener, server, request_cursor);

        struct wlr_seat_pointer_request_set_cursor_event *event =
            (wlr_seat_pointer_request_set_cursor_event *)data;
        struct wlr_seat_client *focused_client =
            server->seat->pointer_state.focused_client;

        // only obey focused client
        if (focused_client == event->seat_client)
            wlr_cursor_set_surface(server->cursor->cursor, event->surface,
                                   event->hotspot_x, event->hotspot_y);
    };
    wl_signal_add(&seat->events.request_set_cursor, &request_cursor);

    // request_set_selection (seat)
    request_set_selection.notify = [](struct wl_listener *listener,
                                      void *data) {
        // user selection
        struct Server *server =
            wl_container_of(listener, server, request_set_selection);

        struct wlr_seat_request_set_selection_event *event =
            (wlr_seat_request_set_selection_event *)data;

        wlr_seat_set_selection(server->seat, event->source, event->serial);
    };
    wl_signal_add(&seat->events.request_set_selection, &request_set_selection);

    // xdg output manager
    wlr_xdg_output_manager =
        wlr_xdg_output_manager_v1_create(wl_display, output_layout);

    // output manager
    wlr_output_manager = wlr_output_manager_v1_create(wl_display);

    // output manager apply
    output_apply.notify = [](struct wl_listener *listener, void *data) {
        Server *server = wl_container_of(listener, server, output_apply);

        server->apply_output_config((wlr_output_configuration_v1 *)data, false);
    };
    wl_signal_add(&wlr_output_manager->events.apply, &output_apply);

    // output manager test
    output_test.notify = [](struct wl_listener *listener, void *data) {
        Server *server = wl_container_of(listener, server, output_apply);

        server->apply_output_config((wlr_output_configuration_v1 *)data, true);
    };
    wl_signal_add(&wlr_output_manager->events.test, &output_test);

    // xwayland shell
    // xwayland_shell = new XWaylandShell(wl_display, scene);

    // screencopy manager
    wlr_screencopy_manager = wlr_screencopy_manager_v1_create(wl_display);

    // foreign toplevel list
    wlr_foreign_toplevel_list =
        wlr_ext_foreign_toplevel_list_v1_create(wl_display, 1);

    // foreign toplevel manager
    wlr_foreign_toplevel_manager =
        wlr_foreign_toplevel_manager_v1_create(wl_display);

    // data control manager
    wlr_data_control_manager = wlr_data_control_manager_v1_create(wl_display);

    // gamma control manager
    wlr_gamma_control_manager = wlr_gamma_control_manager_v1_create(wl_display);
    wlr_scene_set_gamma_control_manager_v1(scene, wlr_gamma_control_manager);

    // image copy capture manager
    ext_image_copy_capture_manager =
        wlr_ext_image_copy_capture_manager_v1_create(wl_display, 1);
    wlr_ext_output_image_capture_source_manager_v1_create(wl_display, 1);

    // fractional scale manager
    wlr_fractional_scale_manager =
        wlr_fractional_scale_manager_v1_create(wl_display, 1);

    // drm syncobj manager
    if (wlr_renderer_get_drm_fd(renderer) >= 0 && renderer->features.timeline &&
        backend->features.timeline) {
        wlr_linux_drm_syncobj_manager_v1_create(
            wl_display, 1, wlr_renderer_get_drm_fd(renderer));
    }

    // unix socket for display
    const char *socket = wl_display_add_socket_auto(wl_display);
    if (!socket) {
        wlr_backend_destroy(backend);
        exit(1);
    }

    // backend start
    if (!wlr_backend_start(backend)) {
        wlr_backend_destroy(backend);
        wl_display_destroy(wl_display);
        exit(1);
    }

    // set wayland diplay to our socket
    setenv("WAYLAND_DISPLAY", socket, true);

    // set envvars from config
    for (std::pair<std::string, std::string> kv : config->startup_env)
        setenv(kv.first.c_str(), kv.second.c_str(), true);

    // run startup commands from config
    for (std::string command : config->startup_commands)
        if (fork() == 0)
            execl("/bin/sh", "/bin/sh", "-c", command.c_str(), nullptr);

    // run event loop
    wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s",
            socket);
    wl_display_run(wl_display);
}

Server::~Server() {
    wl_display_destroy_clients(wl_display);

    wl_list_remove(&new_xdg_toplevel.link);
    wl_list_remove(&new_xdg_popup.link);

    delete cursor;

    wl_list_remove(&new_input.link);
    wl_list_remove(&request_cursor.link);
    wl_list_remove(&request_set_selection.link);

    wl_list_remove(&new_output.link);
    wl_list_remove(&renderer_lost.link);

    wl_list_remove(&output_apply.link);
    wl_list_remove(&output_test.link);

    delete layer_shell;
    // delete xwayland_shell;

    wlr_scene_node_destroy(&scene->tree.node);
    wlr_allocator_destroy(allocator);
    wlr_renderer_destroy(renderer);
    wlr_backend_destroy(backend);
    wl_display_destroy(wl_display);
}
