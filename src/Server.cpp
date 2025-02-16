#include "Server.h"
#include <map>
#include <thread>

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

// get workspace by toplevel
struct Workspace *Server::get_workspace(struct Toplevel *toplevel) {
    Output *output, *tmp;
    wl_list_for_each_safe(output, tmp, &outputs, link) {
        Workspace *workspace, *tmp1;
        wl_list_for_each_safe(
            workspace, tmp1, &output->workspaces,
            link) if (workspace->contains(toplevel)) return workspace;
    }

    return nullptr;
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
    return (T *)(tree->node.data);
}

// find a toplevel by location
struct Toplevel *Server::toplevel_at(double lx, double ly,
                                     struct wlr_surface **surface, double *sx,
                                     double *sy) {
    Toplevel *toplevel = surface_at<Toplevel>(lx, ly, surface, sx, sy);
    if (toplevel && surface && (*surface)->mapped &&
        strcmp((*surface)->role->name, "zwlr_layer_surface_v1") != 0)
        return toplevel;
    else
        return nullptr;
}

// find a layer surface by location
struct LayerSurface *Server::layer_surface_at(double lx, double ly,
                                              struct wlr_surface **surface,
                                              double *sx, double *sy) {
    LayerSurface *layer_surface =
        surface_at<LayerSurface>(lx, ly, surface, sx, sy);
    if (layer_surface && surface && (*surface)->mapped &&
        strcmp((*surface)->role->name, "zwlr_layer_surface_v1") == 0)
        return layer_surface;
    else
        return nullptr;
}

// get output by wlr_output
struct Output *Server::get_output(struct wlr_output *wlr_output) {
    Output *output, *tmp;
    wl_list_for_each_safe(output, tmp, &outputs,
                          link) if (output->wlr_output == wlr_output) {
        return output;
    }
    wlr_log(WLR_ERROR, "could not find output");
    return nullptr;
}

// get the output based on screen coordinates
struct Output *Server::output_at(double x, double y) {
    struct wlr_output *wlr_output =
        wlr_output_layout_output_at(output_layout, x, y);

    if (wlr_output == NULL)
        return NULL;

    return get_output(wlr_output);
}

// get the focused output
struct Output *Server::focused_output() {
    return output_at(cursor->cursor->x, cursor->cursor->y);
}

bool Server::apply_output_config_to_output(Output *output, OutputConfig *config,
                                           bool test_only) {
    struct wlr_output *wlr_output = output->wlr_output;

    struct wlr_output_state state;
    wlr_output_state_init(&state);

    // enabled
    wlr_output_state_set_enabled(&state, config->enabled);

    if (config->enabled) {
        // set mode
        bool mode_set = false;
        if (config->width > 0 && config->height > 0 && config->refresh > 0) {
            // find matching mode
            struct wlr_output_mode *mode, *best_mode = nullptr;
            wl_list_for_each(
                mode, &wlr_output->modes,
                link) if ((mode->width == config->width &&
                           mode->height == config->height) &&
                          (!best_mode ||
                           (abs((int)(mode->refresh / 1000.0 -
                                      config->refresh)) < 1.5 &&
                            abs((int)(mode->refresh / 1000.0 -
                                      config->refresh)) <
                                abs((int)(best_mode->refresh / 1000.0 -
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
            wlr_output_state_set_scale(&state, config->scale);

        // transform
        wlr_output_state_set_transform(&state, config->transform);

        // adaptive sync
        wlr_output_state_set_adaptive_sync_enabled(&state,
                                                   config->adaptive_sync);
    }

    bool success;
    if (test_only)
        success = wlr_output_test_state(wlr_output, &state);
    else {
        success = wlr_output_commit_state(wlr_output, &state);
        if (success) {
            // update position
            wlr_output_layout_add(output_layout, wlr_output, config->x,
                                  config->y);
            // rearrange
            output->arrange_layers();
        }
    }

    wlr_output_state_finish(&state);
    return success;
}

void Server::apply_output_config(struct wlr_output_configuration_v1 *cfg,
                                 bool test_only) {
    // create a map of output names to configs
    std::map<std::string, OutputConfig *> config_map;

    // add existing configs
    for (OutputConfig *c : config->outputs)
        config_map[c->name] = c;

    // oveerride with new configs from cfg
    struct wlr_output_configuration_head_v1 *config_head;
    wl_list_for_each(config_head, &cfg->heads, link) {
        std::string name = config_head->state.output->name;
        config_map[name] = new OutputConfig(config_head);
    }

    // apply each config
    bool success = true;
    Output *output, *tmp;
    wl_list_for_each_safe(output, tmp, &outputs, link) {
        OutputConfig *oc = config_map[output->wlr_output->name];
        success &= apply_output_config_to_output(output, oc, test_only);
    }

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

    update_monitors.notify = [](struct wl_listener *listener, void *data) {
        struct wlr_output_configuration_v1 *config =
            wlr_output_configuration_v1_create();
        struct Output *output;
        struct Server *server =
            wl_container_of(listener, server, update_monitors);

        wl_list_for_each(output, &server->outputs, link) {
            // get the config head for each output
            struct wlr_output_configuration_head_v1 *config_head =
                wlr_output_configuration_head_v1_create(config,
                                                        output->wlr_output);

            // get the output box
            struct wlr_box output_box;
            wlr_output_layout_get_box(server->output_layout, output->wlr_output,
                                      &output_box);

            // mark the output enabled if it's swithed off but not disabled
            config_head->state.enabled = !wlr_box_empty(&output_box);
            config_head->state.x = output_box.x;
            config_head->state.y = output_box.y;
        }

        // update the configuration
        wlr_output_manager_v1_set_configuration(server->wlr_output_manager,
                                                config);
    };
    wl_signal_add(&output_layout->events.change, &update_monitors);

    // new_output
    new_output.notify = [](struct wl_listener *listener, void *data) {
        // new display / monitor available
        struct Server *server = wl_container_of(listener, server, new_output);
        struct wlr_output *wlr_output = static_cast<struct wlr_output *>(data);

        // set allocator and renderer for output
        wlr_output_init_render(wlr_output, server->allocator, server->renderer);

        // add to outputs list
        struct Output *output = new Output(server, wlr_output);
        wl_list_insert(&server->outputs, &output->link);

        // find matching config
        OutputConfig *matching = nullptr;
        for (OutputConfig *config : server->config->outputs) {
            if (config->name == wlr_output->name) {
                matching = config;
                break;
            }
        }

        // apply the config
        bool config_success = matching && server->apply_output_config_to_output(
                                              output, matching, false);

        // fallback
        if (!config_success) {
            wlr_log(WLR_INFO, "using fallback mode for output %s",
                    output->wlr_output->name);

            // create new state
            struct wlr_output_state state;
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
                ? wlr_output_layout_add(server->output_layout, wlr_output,
                                        matching->x, matching->y)
                : wlr_output_layout_add_auto(server->output_layout, wlr_output);

        // add to scene output
        wlr_scene_output *scene_output =
            wlr_scene_output_create(server->scene, wlr_output);
        wlr_scene_output_layout_add_output(server->scene_layout,
                                           output_layout_output, scene_output);

        // set usable area
        wlr_output_layout_get_box(server->output_layout, wlr_output,
                                  &output->usable_area);
    };
    wl_signal_add(&backend->events.new_output, &new_output);

    // scene
    scene = wlr_scene_create();
    scene_layout = wlr_scene_attach_output_layout(scene, output_layout);

    // create xdg shell
    xdg_shell = wlr_xdg_shell_create(wl_display, 6);

    // new_xdg_toplevel
    new_xdg_toplevel.notify = [](struct wl_listener *listener, void *data) {
        struct Server *server =
            wl_container_of(listener, server, new_xdg_toplevel);

        // toplevels are managed by workspaces
        [[maybe_unused]] struct Toplevel *toplevel =
            new Toplevel(server, static_cast<wlr_xdg_toplevel *>(data));
    };
    wl_signal_add(&xdg_shell->events.new_toplevel, &new_xdg_toplevel);

    // new_xdg_popup
    new_xdg_popup.notify = [](struct wl_listener *listener, void *data) {
        // popups do not need to be tracked
        [[maybe_unused]] struct Popup *popup =
            new Popup(static_cast<wlr_xdg_popup *>(data));
    };
    wl_signal_add(&xdg_shell->events.new_popup, &new_xdg_popup);

    // layers
    layers.background = wlr_scene_tree_create(&scene->tree);
    layers.bottom = wlr_scene_tree_create(&scene->tree);
    layers.floating = wlr_scene_tree_create(&scene->tree);
    layers.fullscreen = wlr_scene_tree_create(&scene->tree);
    layers.top = wlr_scene_tree_create(&scene->tree);
    layers.overlay = wlr_scene_tree_create(&scene->tree);

    // layer shell
    wl_list_init(&layer_surfaces);
    wlr_layer_shell = wlr_layer_shell_v1_create(wl_display, 5);

    // new_shell_surface
    new_shell_surface.notify = [](struct wl_listener *listener, void *data) {
        // layer surface created
        Server *server = wl_container_of(listener, server, new_shell_surface);
        wlr_layer_surface_v1 *surface = (wlr_layer_surface_v1 *)data;

        Output *output = nullptr;

        // assume focused output if not set
        if (surface->output) {
            output = server->get_output(surface->output);
        } else {
            output = server->focused_output();

            if (output)
                surface->output = output->wlr_output;
            else {
                wlr_log(WLR_ERROR, "no available output for layer surface");
                return;
            }
        }

        // add to layer surfaces
        LayerSurface *layer_surface = new LayerSurface(output, surface);
        if (layer_surface)
            wl_list_insert(&server->layer_surfaces, &layer_surface->link);
    };
    wl_signal_add(&wlr_layer_shell->events.new_surface, &new_shell_surface);

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

    // cursor
    cursor = new Cursor(this);

    // keyboards
    wl_list_init(&keyboards);

    // new_input
    new_input.notify = [](struct wl_listener *listener, void *data) {
        // create input device based on type
        struct Server *server = wl_container_of(listener, server, new_input);
        struct wlr_input_device *device = static_cast<wlr_input_device *>(data);

        // handle device type
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
            static_cast<wlr_seat_request_set_selection_event *>(data);

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

        server->apply_output_config(
            static_cast<wlr_output_configuration_v1 *>(data), false);
    };
    wl_signal_add(&wlr_output_manager->events.apply, &output_apply);

    // output manager test
    output_test.notify = [](struct wl_listener *listener, void *data) {
        Server *server = wl_container_of(listener, server, output_test);

        server->apply_output_config(
            static_cast<wlr_output_configuration_v1 *>(data), true);
    };
    wl_signal_add(&wlr_output_manager->events.test, &output_test);

    // virtual pointer manager
    virtual_pointer_mgr = wlr_virtual_pointer_manager_v1_create(wl_display);

    new_virtual_pointer.notify = [](struct wl_listener *listener, void *data) {
        Server *server = wl_container_of(listener, server, new_virtual_pointer);

        struct wlr_virtual_pointer_v1_new_pointer_event *event =
            static_cast<wlr_virtual_pointer_v1_new_pointer_event *>(data);
        struct wlr_virtual_pointer_v1 *pointer = event->new_pointer;
        struct wlr_input_device *device = &pointer->pointer.base;

        wlr_cursor_attach_input_device(server->cursor->cursor, device);
        if (event->suggested_output)
            wlr_cursor_map_input_to_output(server->cursor->cursor, device,
                                           event->suggested_output);
    };
    wl_signal_add(&virtual_pointer_mgr->events.new_virtual_pointer,
                  &new_virtual_pointer);

    // xwayland shell
    // xwayland_shell = new XWaylandShell(wl_display, scene);

    // export dmabuf manager
    wlr_export_dmabuf_manager = wlr_export_dmabuf_manager_v1_create(wl_display);

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

    // avoid using "wayland-0" as display socket
    std::string socket;
    for (unsigned int i = 1; i <= 32; i++) {
        socket = "wayland-" + std::to_string(i);
        int ret = wl_display_add_socket(wl_display, socket.c_str());
        if (!ret) {
            break;
        } else {
            wlr_log(WLR_ERROR,
                    "wl_display_add_socket for %s returned %d: skipping",
                    socket.c_str(), ret);
        }
    }

    if (!socket.c_str()) {
        wlr_log(WLR_DEBUG, "Unable to open wayland socket");
        wlr_backend_destroy(backend);
        return;
    }

    // backend start
    if (!wlr_backend_start(backend)) {
        wlr_backend_destroy(backend);
        wl_display_destroy(wl_display);
        exit(1);
    }

    // set up SIGCHLD handler to reap zombie processes
    struct sigaction sa;
    sa.sa_handler = [](int sig) {
        while (waitpid(-1, NULL, WNOHANG) > 0)
            ;
    };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NOCLDSTOP | SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    // set wayland diplay to our socket
    setenv("WAYLAND_DISPLAY", socket.c_str(), true);

    // set envvars from config
    for (std::pair<std::string, std::string> kv : config->startup_env)
        setenv(kv.first.c_str(), kv.second.c_str(), true);

    // run startup commands from config
    for (std::string command : config->startup_commands)
        if (fork() == 0)
            execl("/bin/sh", "/bin/sh", "-c", command.c_str(), nullptr);

    // linux dmabuf
    if (wlr_renderer_get_texture_formats(renderer, WLR_BUFFER_CAP_DMABUF)) {
        wlr_linux_dmabuf =
            wlr_linux_dmabuf_v1_create_with_renderer(wl_display, 4, renderer);
        wlr_scene_set_linux_dmabuf_v1(scene, wlr_linux_dmabuf);
    }

    // run thread for config updater
    std::thread config_thread([&]() {
        while (true) {
            // update config
            config->update();

            // sleep
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
    config_thread.detach();

    // run event loop
    wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s",
            socket.c_str());
    wl_display_run(wl_display);

    // close thread
    config_thread.join();
}

void Server::arrange() {
    struct Output *output, *tmp;
    wl_list_for_each_safe(output, tmp, &outputs, link) {
        output->update_position();
        /*
        wlr_scene_output_set_position(output->scene_output, output->lx,
        output->ly) wlr_scene_node_reparent(&output->layers.background->node,
        shell_background); wlr_scene_node_reparent(&output->layers.bottom->node,
        shell_bottom); wlr_scene_node_reparent(&output->layers.top->node,
        shell_top); wlr_scene_node_reparent(&output->layers.overlay->node,
        shell_overlay);
        */
        wlr_scene_node_set_position(&output->layers.background->node,
                                    output->lx, output->ly);
        wlr_scene_node_set_position(&output->layers.bottom->node, output->lx,
                                    output->ly);
        wlr_scene_node_set_position(&output->layers.top->node, output->lx,
                                    output->ly);
        wlr_scene_node_set_position(&output->layers.overlay->node, output->lx,
                                    output->ly);
    }
}

Server::~Server() {
    wl_display_destroy_clients(wl_display);

    wl_list_remove(&new_xdg_toplevel.link);
    wl_list_remove(&new_xdg_popup.link);

    delete cursor;

    wl_list_remove(&new_input.link);
    wl_list_remove(&request_cursor.link);
    wl_list_remove(&request_set_selection.link);

    wl_list_remove(&update_monitors.link);
    wl_list_remove(&new_output.link);
    wl_list_remove(&renderer_lost.link);

    wl_list_remove(&output_apply.link);
    wl_list_remove(&output_test.link);

    wl_list_remove(&new_shell_surface.link);
    wl_list_remove(&new_virtual_pointer.link);

    struct LayerSurface *surface, *tmp;
    wl_list_for_each_safe(surface, tmp, &layer_surfaces, link) delete surface;
    // delete xwayland_shell;

    wlr_scene_node_destroy(&scene->tree.node);
    wlr_allocator_destroy(allocator);
    wlr_renderer_destroy(renderer);
    wlr_backend_destroy(backend);
    wl_display_destroy(wl_display);
}
