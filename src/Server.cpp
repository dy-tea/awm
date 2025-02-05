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
    wlr_cursor_attach_input_device(cursor, device);
}

// get a node tree surface from its location and cast it to the generic
// type provided
template <typename T>
T *Server::surface_at(double lx, double ly, struct wlr_surface **surface,
                      double *sx, double *sy) {
    // get the scene node and ensure it's a buffer
    struct wlr_scene_node *node =
        wlr_scene_node_at(&scene->tree.node, lx, ly, sx, sy);
    if (node == NULL || node->type != WLR_SCENE_NODE_BUFFER)
        return NULL;

    // get the scene buffer and surface of the node
    struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
    struct wlr_scene_surface *scene_surface =
        wlr_scene_surface_try_from_buffer(scene_buffer);
    if (!scene_surface)
        return NULL;

    // set the scene surface
    *surface = scene_surface->surface;

    // get the scene tree of the node's parent
    struct wlr_scene_tree *tree = node->parent;
    if (tree->node.type != WLR_SCENE_NODE_TREE)
        return NULL;

    // find the topmost node of the scene tree
    while (tree != NULL && tree->node.data == NULL)
        tree = tree->node.parent;

    // invalid tree
    if (tree == NULL)
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
    wl_list *out = &outputs;

    for (uint32_t i = 0; i != index && out->next != nullptr; ++i)
        out = out->next;

    Output *o = wl_container_of(out, o, link);
    return o;
}

// unactivated cursor
void Server::reset_cursor_mode() {
    cursor_mode = CURSORMODE_PASSTHROUGH;
    grabbed_toplevel = NULL;
}

// move a toplevel
void Server::process_cursor_move() {
    if (grabbed_toplevel->xdg_toplevel->current.fullscreen)
        return;

    wlr_scene_node_set_position(&grabbed_toplevel->scene_tree->node,
                                cursor->x - grab_x, cursor->y - grab_y);
}

// resize a toplevel
void Server::process_cursor_resize() {
    // tinywl resize
    struct Toplevel *toplevel = grabbed_toplevel;
    double border_x = cursor->x - grab_x;
    double border_y = cursor->y - grab_y;
    int new_left = grab_geobox.x;
    int new_right = grab_geobox.x + grab_geobox.width;
    int new_top = grab_geobox.y;
    int new_bottom = grab_geobox.y + grab_geobox.height;

    if (resize_edges & WLR_EDGE_TOP) {
        new_top = border_y;
        if (new_top >= new_bottom)
            new_top = new_bottom - 1;
    } else if (resize_edges & WLR_EDGE_BOTTOM) {
        new_bottom = border_y;
        if (new_bottom <= new_top)
            new_bottom = new_top + 1;
    }
    if (resize_edges & WLR_EDGE_LEFT) {
        new_left = border_x;
        if (new_left >= new_right)
            new_left = new_right - 1;
    } else if (resize_edges & WLR_EDGE_RIGHT) {
        new_right = border_x;
        if (new_right <= new_left)
            new_right = new_left + 1;
    }

    struct wlr_box *geo_box = &toplevel->xdg_toplevel->base->geometry;
    wlr_scene_node_set_position(&toplevel->scene_tree->node,
                                new_left - geo_box->x, new_top - geo_box->y);

    wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, new_right - new_left,
                              new_bottom - new_top);
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

void Server::process_cursor_motion(uint32_t time) {
    // move or resize
    if (cursor_mode == CURSORMODE_MOVE) {
        process_cursor_move();
        return;
    } else if (cursor_mode == CURSORMODE_RESIZE) {
        process_cursor_resize();
        return;
    }

    // otherwise mode is passthrough
    double sx, sy;
    struct wlr_surface *surface = NULL;

    // get the toplevel under the cursor (if exists)
    struct Toplevel *toplevel =
        toplevel_at(cursor->x, cursor->y, &surface, &sx, &sy);
    if (toplevel)
        if (surface) {
            // connect the seat to the toplevel
            wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
            wlr_seat_pointer_notify_motion(seat, time, sx, sy);
            return;
        }

    // get the layer surface under the cursor (if exists)
    struct LayerSurface *layer_surface =
        layer_surface_at(cursor->x, cursor->y, &surface, &sx, &sy);
    if (layer_surface)
        if (surface) {
            // connect the seat to the layer surface
            wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
            wlr_seat_pointer_notify_motion(seat, time, sx, sy);
            return;
        }

    // set default cursor mode
    wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");
    wlr_seat_pointer_clear_focus(seat);
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

    // new_output
    new_output.notify = [](struct wl_listener *listener, void *data) {
        // new display / monitor available
        struct Server *server = wl_container_of(listener, server, new_output);
        struct wlr_output *wlr_output = static_cast<struct wlr_output *>(data);

        // set allocator and renderer for output
        wlr_output_init_render(wlr_output, server->allocator, server->renderer);

        // turn on output if off
        struct wlr_output_state state;
        wlr_output_state_init(&state);
        wlr_output_state_set_enabled(&state, true);

        // use monitor's preferred mode
        struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
        if (mode != NULL)
            wlr_output_state_set_mode(&state, mode);

        // apply output state
        wlr_output_commit_state(wlr_output, &state);
        wlr_output_state_finish(&state);

        // add to outputs list
        struct Output *output = new Output(server, wlr_output);
        wl_list_insert(&server->outputs, &output->link);
    };
    wl_signal_add(&backend->events.new_output, &new_output);

    // scene
    scene = wlr_scene_create();
    scene_layout = wlr_scene_attach_output_layout(scene, output_layout);

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
    cursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(cursor, output_layout);
    cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
    cursor_mode = CURSORMODE_PASSTHROUGH;

    // cursor_motion
    cursor_motion.notify = [](struct wl_listener *listener, void *data) {
        // relative motion event
        struct Server *server =
            wl_container_of(listener, server, cursor_motion);
        struct wlr_pointer_motion_event *event =
            (wlr_pointer_motion_event *)data;

        wlr_cursor_move(server->cursor, &event->pointer->base, event->delta_x,
                        event->delta_y);
        server->process_cursor_motion(event->time_msec);
    };
    wl_signal_add(&cursor->events.motion, &cursor_motion);

    // cursor_motion_absolute
    cursor_motion_absolute.notify = [](struct wl_listener *listener,
                                       void *data) {
        // absolute motion event
        struct Server *server =
            wl_container_of(listener, server, cursor_motion_absolute);
        struct wlr_pointer_motion_absolute_event *event =
            (wlr_pointer_motion_absolute_event *)data;

        wlr_cursor_warp_absolute(server->cursor, &event->pointer->base,
                                 event->x, event->y);
        server->process_cursor_motion(event->time_msec);
    };
    wl_signal_add(&cursor->events.motion_absolute, &cursor_motion_absolute);

    // cursor_button
    cursor_button.notify = [](struct wl_listener *listener, void *data) {
        struct Server *server =
            wl_container_of(listener, server, cursor_button);
        struct wlr_pointer_button_event *event =
            (wlr_pointer_button_event *)data;

        // forward to seat
        wlr_seat_pointer_notify_button(server->seat, event->time_msec,
                                       event->button, event->state);

        if (event->state == WL_POINTER_BUTTON_STATE_RELEASED)
            server->reset_cursor_mode();
        else {
            // focus surface
            double sx, sy;
            struct wlr_surface *surface = NULL;
            struct LayerSurface *layer_surface = server->layer_surface_at(
                server->cursor->x, server->cursor->y, &surface, &sx, &sy);
            if (layer_surface && surface)
                if (layer_surface->should_focus()) {
                    layer_surface->handle_focus();
                    return;
                }

            struct Toplevel *toplevel = server->toplevel_at(
                server->cursor->x, server->cursor->y, &surface, &sx, &sy);
            if (toplevel != NULL)
                toplevel->focus();
        }
    };
    wl_signal_add(&cursor->events.button, &cursor_button);

    // cursor_axis
    cursor_axis.notify = [](struct wl_listener *listener, void *data) {
        // scroll wheel etc
        struct Server *server = wl_container_of(listener, server, cursor_axis);

        struct wlr_pointer_axis_event *event = (wlr_pointer_axis_event *)data;

        // forward to seat
        wlr_seat_pointer_notify_axis(
            server->seat, event->time_msec, event->orientation, event->delta,
            event->delta_discrete, event->source, event->relative_direction);
    };
    wl_signal_add(&cursor->events.axis, &cursor_axis);

    // cursor_frame
    cursor_frame.notify = [](struct wl_listener *listener, void *data) {
        struct Server *server = wl_container_of(listener, server, cursor_frame);

        // forward to seat
        wlr_seat_pointer_notify_frame(server->seat);
    };
    wl_signal_add(&cursor->events.frame, &cursor_frame);

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
            wlr_cursor_set_surface(server->cursor, event->surface,
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

    // layer shell
    layer_shell = new LayerShell(wl_display, scene, seat);

    // xdg output manager
    wlr_xdg_output_manager =
        wlr_xdg_output_manager_v1_create(wl_display, output_layout);

    // xwayland shell
    // xwayland_shell = new XWaylandShell(wl_display, scene);

    // screencopy manager
    wlr_screencopy_manager = wlr_screencopy_manager_v1_create(wl_display);

    // foreign toplevel manager
    wlr_foreign_toplevel_manager =
        wlr_foreign_toplevel_manager_v1_create(wl_display);

    // data control manager
    wlr_data_control_manager = wlr_data_control_manager_v1_create(wl_display);

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

    wl_list_remove(&cursor_motion.link);
    wl_list_remove(&cursor_motion_absolute.link);
    wl_list_remove(&cursor_button.link);
    wl_list_remove(&cursor_axis.link);
    wl_list_remove(&cursor_frame.link);

    wl_list_remove(&new_input.link);
    wl_list_remove(&request_cursor.link);
    wl_list_remove(&request_set_selection.link);

    wl_list_remove(&new_output.link);
    wl_list_remove(&renderer_lost.link);

    delete layer_shell;
    // delete xwayland_shell;

    wlr_scene_node_destroy(&scene->tree.node);
    wlr_xcursor_manager_destroy(cursor_mgr);
    wlr_cursor_destroy(cursor);
    wlr_allocator_destroy(allocator);
    wlr_renderer_destroy(renderer);
    wlr_backend_destroy(backend);
    wl_display_destroy(wl_display);
}
