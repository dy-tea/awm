#include "Server.h"

// get workspace by toplevel
Workspace *Server::get_workspace(Toplevel *toplevel) const {
    Output *output, *tmp;
    Workspace *workspace, *tmp1;

    // check each output
    // for each output check each workspace
    wl_list_for_each_safe(output, tmp, &output_manager->outputs, link)
        wl_list_for_each_safe(
            workspace, tmp1, &output->workspaces,
            link) if (workspace->contains(toplevel)) return workspace;

    // no workspace found
    return nullptr;
}

// get a node tree surface from its location and cast it to the generic
// type provided
template <typename T>
T *Server::surface_at(const double lx, const double ly, wlr_surface **surface,
                      double *sx, double *sy) {
    // get the scene node and ensure it's a buffer
    wlr_scene_node *node = wlr_scene_node_at(&scene->tree.node, lx, ly, sx, sy);
    if (!node || node->type != WLR_SCENE_NODE_BUFFER)
        return nullptr;

    // get the scene buffer and surface of the node
    wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
    wlr_scene_surface *scene_surface =
        wlr_scene_surface_try_from_buffer(scene_buffer);
    if (!scene_surface || !scene_surface->surface)
        return nullptr;

    // set the scene surface
    *surface = scene_surface->surface;

    // get the scene tree of the node's parent
    wlr_scene_tree *tree = node->parent;
    if (!tree || tree->node.type != WLR_SCENE_NODE_TREE)
        return nullptr;

    // find the topmost node of the scene tree
    while (tree && !tree->node.data)
        tree = tree->node.parent;

    // invalid tree
    if (!tree || !tree->node.parent)
        return nullptr;

    // return the topmost node's data
    return static_cast<T *>(tree->node.data);
}

// find a toplevel by location
Toplevel *Server::toplevel_at(const double lx, const double ly,
                              wlr_surface **surface, double *sx, double *sy) {
    Toplevel *toplevel = surface_at<Toplevel>(lx, ly, surface, sx, sy);

    // ensure role is not layer surface
    if (toplevel && surface && (*surface)->mapped &&
        strcmp((*surface)->role->name, "zwlr_layer_surface_v1") != 0)
        return toplevel;

    return nullptr;
}

// find a layer surface by location
LayerSurface *Server::layer_surface_at(const double lx, const double ly,
                                       wlr_surface **surface, double *sx,
                                       double *sy) {
    LayerSurface *layer_surface =
        surface_at<LayerSurface>(lx, ly, surface, sx, sy);

    // ensure role is layer surface
    if (layer_surface && surface && (*surface)->mapped &&
        strcmp((*surface)->role->name, "zwlr_layer_surface_v1") == 0)
        return layer_surface;

    return nullptr;
}

// get output by wlr_output
Output *Server::get_output(const wlr_output *wlr_output) const {
    return output_manager->get_output(wlr_output);
}

// get toplevel by wlr_surface
Toplevel *Server::get_toplevel(wlr_surface *surface) const {
    // check each toplevel in each workspace in each output
    // get the toplevel by the surface
    // optionally get xwayland toplevels
    Output *output, *tmp;
    Workspace *workspace, *tmp1;
    Toplevel *toplevel, *tmp2;
    wl_list_for_each_safe(output, tmp, &output_manager->outputs, link)
        wl_list_for_each_safe(workspace, tmp1, &output->workspaces, link)
            wl_list_for_each_safe(
                toplevel, tmp2, &workspace->toplevels,
                link) if ((toplevel->xdg_toplevel &&
                           toplevel->xdg_toplevel->base->surface == surface)
#ifdef XWAYLAND
                          || (toplevel->xwayland_surface &&
                              toplevel->xwayland_surface->surface == surface)
#endif
                              ) return toplevel;

    return nullptr;
}

// get the output under the cursor
Output *Server::focused_output() const {
    return output_manager->output_at(cursor->cursor->x, cursor->cursor->y);
}

// execute either a wm bind or command bind, returns true if
// bind is valid, false otherwise
bool Server::handle_bind(Bind bind) {
    // get current output
    Output *output = focused_output();
    if (!output)
        return false;

    // digit pressed
    int n = 1;

    // handle digits
    if (bind.sym >= XKB_KEY_0 && bind.sym <= XKB_KEY_9) {
        // 0 is on the right of 9 so it makes more sense this way
        n = XKB_KEY_0 == bind.sym ? 10 : bind.sym - XKB_KEY_0;

        // digit pressed
        bind.sym = XKB_KEY_NoSymbol;
    }

    // locate in wm binds
    for (Bind b : config->binds)
        if (b == bind) {
            bind.name = b.name;
            break;
        }

    switch (bind.name) {
    case BIND_EXIT:
        // exit compositor
        exit();
        break;
    case BIND_WINDOW_MAXIMIZE: {
        // maximize the active toplevel
        Toplevel *active = output->get_active()->active_toplevel;

        if (!active)
            return false;

        active->toggle_maximized();
        break;
    }
    case BIND_WINDOW_FULLSCREEN: {
        // fullscreen the active toplevel
        Toplevel *active = output->get_active()->active_toplevel;

        if (!active)
            return false;

        active->toggle_fullscreen();
        break;
    }
    case BIND_WINDOW_PREVIOUS:
        // focus the previous toplevel in the active workspace
        output->get_active()->focus_prev();
        break;
    case BIND_WINDOW_NEXT:
        // focus the next toplevel in the active workspace
        output->get_active()->focus_next();
        break;
    case BIND_WINDOW_MOVE:
        // move the active toplevel with the mouse
        if (Toplevel *active = output->get_active()->active_toplevel)
            active->begin_interactive(CURSORMODE_MOVE, 0);
        break;
    case BIND_WINDOW_UP:
        // focus the toplevel in the up direction
        if (Toplevel *up = output->get_active()->in_direction(WLR_DIRECTION_UP))
            output->get_active()->focus_toplevel(up);
        break;
    case BIND_WINDOW_DOWN:
        // focus the toplevel in the down direction
        if (Toplevel *down =
                output->get_active()->in_direction(WLR_DIRECTION_DOWN))
            output->get_active()->focus_toplevel(down);
        break;
    case BIND_WINDOW_LEFT:
        // focus the toplevel in the left direction
        if (Toplevel *left =
                output->get_active()->in_direction(WLR_DIRECTION_LEFT))
            output->get_active()->focus_toplevel(left);
        break;
    case BIND_WINDOW_RIGHT:
        // focus the toplevel in the right direction
        if (Toplevel *right =
                output->get_active()->in_direction(WLR_DIRECTION_RIGHT))
            output->get_active()->focus_toplevel(right);
        break;
    case BIND_WINDOW_CLOSE:
        // close the active toplevel
        output->get_active()->close_active();
        break;
    case BIND_WINDOW_SWAP_UP:
        // swap the active toplevel with the one above it
        if (Toplevel *other =
                output->get_active()->in_direction(WLR_DIRECTION_UP))
            output->get_active()->swap(other);
        break;
    case BIND_WINDOW_SWAP_DOWN:
        // swap the active toplevel with the one below it
        if (Toplevel *other =
                output->get_active()->in_direction(WLR_DIRECTION_DOWN))
            output->get_active()->swap(other);
        break;
    case BIND_WINDOW_SWAP_LEFT:
        // swap the active toplevel with the one to the left of it
        if (Toplevel *other =
                output->get_active()->in_direction(WLR_DIRECTION_LEFT))
            output->get_active()->swap(other);
        break;
    case BIND_WINDOW_SWAP_RIGHT:
        // swap the active toplevel with the one to the right of it
        if (Toplevel *other =
                output->get_active()->in_direction(WLR_DIRECTION_RIGHT))
            output->get_active()->swap(other);
        break;
    case BIND_WORKSPACE_TILE:
        // set workspace to tile
        output->get_active()->tile();
        break;
    case BIND_WORKSPACE_OPEN:
        // open workspace n
        return output->set_workspace(n);
    case BIND_WORKSPACE_WINDOW_TO: {
        // move active toplevel to workspace n
        Workspace *current = output->get_active();
        Workspace *target = output->get_workspace(n);

        if (target == nullptr)
            return false;

        if (current->active_toplevel)
            current->move_to(current->active_toplevel, target);
        break;
    }
    case BIND_NONE:
    default: {
        // handle user-defined binds
        for (const auto &[cmd_bind, cmd] : config->commands)
            if (cmd_bind == bind)
                if (fork() == 0) {
                    execl("/bin/sh", "/bin/sh", "-c", cmd.c_str(), nullptr);
                    return true;
                }

        return false;
    }
    }

    return true;
}

Server::Server(Config *config) : config(config) {
    // set renderer
    setenv("WLR_RENDERER", config->renderer.c_str(), true);

    // display
    display = wl_display_create();

    // backend
    backend =
        wlr_backend_autocreate(wl_display_get_event_loop(display), &session);
    if (!backend) {
        wlr_log(WLR_ERROR, "%s", "failed to create wlr_backend");
        ::exit(1);
    }

    // renderer
    renderer = wlr_renderer_autocreate(backend);
    if (!renderer) {
        wlr_log(WLR_ERROR, "%s", "failed to create wlr_renderer");
        ::exit(1);
    }

    // wl_shm
    wlr_renderer_init_wl_shm(renderer, display);

    // linux dmabuf
    if (wlr_renderer_get_texture_formats(renderer, WLR_BUFFER_CAP_DMABUF)) {
        wlr_drm_create(display, renderer);
        wlr_linux_dmabuf =
            wlr_linux_dmabuf_v1_create_with_renderer(display, 4, renderer);
    }

    // drm syncobj manager
    if ((drm_fd = wlr_renderer_get_drm_fd(renderer)) >= 0 &&
        renderer->features.timeline && backend->features.timeline)
        wlr_linux_drm_syncobj_manager_v1_create(display, 1, drm_fd);

    // render allocator
    allocator = wlr_allocator_autocreate(backend, renderer);
    if (!allocator) {
        wlr_log(WLR_ERROR, "%s", "failed to create wlr_allocator");
        ::exit(1);
    }

    // wlr compositor
    compositor = wlr_compositor_create(display, 6, renderer);
    wlr_subcompositor_create(display);
    wlr_data_device_manager_create(display);

    // output manager
    output_manager = new OutputManager(this);

    // scene
    scene = wlr_scene_create();
    scene_layout =
        wlr_scene_attach_output_layout(scene, output_manager->layout);

    // attach dmabuf to scene
    if (wlr_linux_dmabuf)
        wlr_scene_set_linux_dmabuf_v1(scene, wlr_linux_dmabuf);

    // create xdg shell
    xdg_shell = wlr_xdg_shell_create(display, 6);

    // new_xdg_toplevel
    new_xdg_toplevel.notify = [](wl_listener *listener, void *data) {
        Server *server = wl_container_of(listener, server, new_xdg_toplevel);

        // toplevels are managed by workspaces
        new Toplevel(server, static_cast<wlr_xdg_toplevel *>(data));
    };
    wl_signal_add(&xdg_shell->events.new_toplevel, &new_xdg_toplevel);

    // layers
    layers.background = wlr_scene_tree_create(&scene->tree);
    layers.bottom = wlr_scene_tree_create(&scene->tree);
    layers.floating = wlr_scene_tree_create(&scene->tree);
    layers.top = wlr_scene_tree_create(&scene->tree);
    layers.fullscreen = wlr_scene_tree_create(&scene->tree);
    layers.overlay = wlr_scene_tree_create(&scene->tree);
    layers.drag_icon = wlr_scene_tree_create(&scene->tree);
    layers.lock = wlr_scene_tree_create(&scene->tree);

    // layer shell
    wl_list_init(&layer_surfaces);
    wlr_layer_shell = wlr_layer_shell_v1_create(display, 5);

    // new_shell_surface
    new_shell_surface.notify = [](wl_listener *listener, void *data) {
        // layer surface created
        Server *server = wl_container_of(listener, server, new_shell_surface);
        auto *surface = static_cast<wlr_layer_surface_v1 *>(data);

        Output *output = nullptr;

        // assume focused output if not set
        if (surface->output)
            output = server->get_output(surface->output);
        else {
            output = server->focused_output();

            if (output)
                surface->output = output->wlr_output;
            else {
                wlr_log(WLR_ERROR, "%s",
                        "no available output for layer surface");
                wlr_layer_surface_v1_destroy(surface);
                return;
            }
        }

        // add to layer surfaces
        LayerSurface *layer_surface = new LayerSurface(output, surface);
        wl_list_insert(&server->layer_surfaces, &layer_surface->link);
    };
    wl_signal_add(&wlr_layer_shell->events.new_surface, &new_shell_surface);

    // renderer_lost
    renderer_lost.notify = [](wl_listener *listener,
                              [[maybe_unused]] void *data) {
        // renderer recovery (thanks sway)
        Server *server = wl_container_of(listener, server, renderer_lost);

        wlr_log(WLR_INFO, "%s", "Re-creating renderer after GPU reset");

        // create new renderer
        wlr_renderer *renderer = wlr_renderer_autocreate(server->backend);
        if (!renderer) {
            wlr_log(WLR_ERROR, "%s", "Unable to create renderer");
            return;
        }

        // create new allocator
        wlr_allocator *allocator =
            wlr_allocator_autocreate(server->backend, renderer);
        if (!allocator) {
            wlr_log(WLR_ERROR, "%s", "Unable to create allocator");
            wlr_renderer_destroy(renderer);
            return;
        }

        // replace old and renderer and allocator
        wlr_renderer *old_renderer = server->renderer;
        wlr_allocator *old_allocator = server->allocator;
        server->renderer = renderer;
        server->allocator = allocator;

        // reset signal
        wl_list_remove(&server->renderer_lost.link);
        wl_signal_add(&server->renderer->events.lost, &server->renderer_lost);

        // move compositor to new renderer
        wlr_compositor_set_renderer(server->compositor, renderer);

        // reinint outputs
        Output *output, *tmp;
        wl_list_for_each_safe(output, tmp, &server->output_manager->outputs,
                              link)
            wlr_output_init_render(output->wlr_output, server->allocator,
                                   server->renderer);

        // destroy old renderer and allocator
        wlr_allocator_destroy(old_allocator);
        wlr_renderer_destroy(old_renderer);
    };
    wl_signal_add(&renderer->events.lost, &renderer_lost);

    // session lock
    wlr_session_lock_manager = wlr_session_lock_manager_v1_create(display);

    new_session_lock.notify = [](wl_listener *listener, void *data) {
        Server *server = wl_container_of(listener, server, new_session_lock);
        auto session_lock = static_cast<wlr_session_lock_v1 *>(data);

        if (server->current_session_lock) {
            wlr_session_lock_v1_destroy(session_lock);
            return;
        }

        new SessionLock(server, session_lock);
    };
    wl_signal_add(&wlr_session_lock_manager->events.new_lock,
                  &new_session_lock);

    // relative pointer
    wlr_relative_pointer_manager =
        wlr_relative_pointer_manager_v1_create(display);

    // cursor
    cursor = new Cursor(this);

    // keyboards
    wl_list_init(&keyboards);

    // new_input
    new_input.notify = [](wl_listener *listener, void *data) {
        // create input device based on type
        Server *server = wl_container_of(listener, server, new_input);

        // handle device type
        switch (auto *device = static_cast<wlr_input_device *>(data);
                device->type) {
        case WLR_INPUT_DEVICE_KEYBOARD: {
            // create keyboard
            Keyboard *keyboard =
                new Keyboard(server, wlr_keyboard_from_input_device(device));

            // set destroy listener
            wl_signal_add(&device->events.destroy, &keyboard->destroy);
            break;
        }
        case WLR_INPUT_DEVICE_POINTER: {
            const auto pointer = reinterpret_cast<wlr_pointer *>(device);

            // set the cursor configuration
            server->cursor->set_config(pointer);

            // attach to device
            wlr_cursor_attach_input_device(server->cursor->cursor,
                                           &pointer->base);
            break;
        }
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
    seat = wlr_seat_create(display, "seat0");

    // request_cursor (seat)
    request_cursor.notify = [](wl_listener *listener, void *data) {
        // client-provided cursor image
        Server *server = wl_container_of(listener, server, request_cursor);

        const auto *event =
            static_cast<wlr_seat_pointer_request_set_cursor_event *>(data);
        wlr_seat_client *focused_client =
            server->seat->pointer_state.focused_client;

        // only obey focused client
        if (focused_client == event->seat_client)
            wlr_cursor_set_surface(server->cursor->cursor, event->surface,
                                   event->hotspot_x, event->hotspot_y);
    };
    wl_signal_add(&seat->events.request_set_cursor, &request_cursor);

    // request_set_selection (seat)
    request_set_selection.notify = [](wl_listener *listener, void *data) {
        // user selection
        Server *server =
            wl_container_of(listener, server, request_set_selection);

        const auto *event =
            static_cast<wlr_seat_request_set_selection_event *>(data);

        wlr_seat_set_selection(server->seat, event->source, event->serial);
    };
    wl_signal_add(&seat->events.request_set_selection, &request_set_selection);

    // request_start_drag (seat)
    request_start_drag.notify = [](wl_listener *listener, void *data) {
        // start drag
        Server *server = wl_container_of(listener, server, request_start_drag);

        const auto *event =
            static_cast<wlr_seat_request_start_drag_event *>(data);

        if (wlr_seat_validate_pointer_grab_serial(server->seat, event->origin,
                                                  event->serial))
            wlr_seat_start_pointer_drag(server->seat, event->drag,
                                        event->serial);
        else
            wlr_data_source_destroy(event->drag->source);
    };
    wl_signal_add(&seat->events.request_start_drag, &request_start_drag);

    // start_drag (seat)
    start_drag.notify = [](wl_listener *listener, void *data) {
        // start drag
        Server *server = wl_container_of(listener, server, start_drag);

        const auto *drag = static_cast<wlr_drag *>(data);
        if (!drag->icon)
            return;

        // create a drag icon in the scene
        drag->icon->data =
            &wlr_scene_drag_icon_create(server->layers.drag_icon, drag->icon)
                 ->node;

        // set up destroy listener
        server->destroy_drag_icon.notify = [](wl_listener *listener,
                                              [[maybe_unused]] void *data) {
            Server *server =
                wl_container_of(listener, server, destroy_drag_icon);
            wl_list_remove(&server->destroy_drag_icon.link);
        };
    };
    wl_signal_add(&seat->events.start_drag, &start_drag);

    // virtual pointer
    virtual_pointer_mgr = wlr_virtual_pointer_manager_v1_create(display);

    new_virtual_pointer.notify = [](wl_listener *listener, void *data) {
        Server *server = wl_container_of(listener, server, new_virtual_pointer);

        const auto *event =
            static_cast<wlr_virtual_pointer_v1_new_pointer_event *>(data);
        wlr_virtual_pointer_v1 *pointer = event->new_pointer;
        wlr_input_device *device = &pointer->pointer.base;

        wlr_cursor_attach_input_device(server->cursor->cursor, device);
        if (event->suggested_output)
            wlr_cursor_map_input_to_output(server->cursor->cursor, device,
                                           event->suggested_output);
    };
    wl_signal_add(&virtual_pointer_mgr->events.new_virtual_pointer,
                  &new_virtual_pointer);

    // virtual keyboard
    virtual_keyboard_manager = wlr_virtual_keyboard_manager_v1_create(display);

    new_virtual_keyboard.notify = [](wl_listener *listener, void *data) {
        Server *server =
            wl_container_of(listener, server, new_virtual_keyboard);

        auto *virtual_keyboard = static_cast<wlr_virtual_keyboard_v1 *>(data);
        Keyboard *keyboard = new Keyboard(server, &virtual_keyboard->keyboard);

        // set up destroy listener
        wl_signal_add(&virtual_keyboard->keyboard.base.events.destroy,
                      &keyboard->destroy);
    };
    wl_signal_add(&virtual_keyboard_manager->events.new_virtual_keyboard,
                  &new_virtual_keyboard);

    // pointer constraints
    wlr_pointer_constraints = wlr_pointer_constraints_v1_create(display);

    new_pointer_constraint.notify = [](wl_listener *listener, void *data) {
        Server *server =
            wl_container_of(listener, server, new_pointer_constraint);

        new PointerConstraint(static_cast<wlr_pointer_constraint_v1 *>(data),
                              server->cursor);
    };
    wl_signal_add(&wlr_pointer_constraints->events.new_constraint,
                  &new_pointer_constraint);

    // xdg activation
    wlr_xdg_activation = wlr_xdg_activation_v1_create(display);

    xdg_activation_activate.notify = [](wl_listener *listener, void *data) {
        Server *server =
            wl_container_of(listener, server, xdg_activation_activate);
        const auto event =
            static_cast<wlr_xdg_activation_v1_request_activate_event *>(data);

        // get toplevel associated with surface
        Toplevel *toplevel = server->get_toplevel(event->surface);
        if (!toplevel || !toplevel->xdg_toplevel ||
            !toplevel->xdg_toplevel->base->surface->mapped)
            return;

        // set workspace to toplevel's workspace
        Workspace *workspace = server->get_workspace(toplevel);
        Output *output = workspace->output;
        if (output->get_active() != workspace)
            output->set_workspace(workspace->num);

        // focus toplevel
        toplevel->focus();
    };
    wl_signal_add(&wlr_xdg_activation->events.request_activate,
                  &xdg_activation_activate);

    // viewporter
    wlr_viewporter = wlr_viewporter_create(display);

    // presentation
    wlr_presentation = wlr_presentation_create(display, backend, 2);

    // export dmabuf manager
    wlr_export_dmabuf_manager = wlr_export_dmabuf_manager_v1_create(display);

    // screencopy manager
    wlr_screencopy_manager = wlr_screencopy_manager_v1_create(display);

    // foreign toplevel list
    wlr_foreign_toplevel_list =
        wlr_ext_foreign_toplevel_list_v1_create(display, 1);

    // foreign toplevel manager
    wlr_foreign_toplevel_manager =
        wlr_foreign_toplevel_manager_v1_create(display);

    // data control manager
    wlr_data_control_manager = wlr_data_control_manager_v1_create(display);

    // gamma control manager
    wlr_gamma_control_manager = wlr_gamma_control_manager_v1_create(display);
    wlr_scene_set_gamma_control_manager_v1(scene, wlr_gamma_control_manager);

    // image copy capture manager
    ext_image_copy_capture_manager =
        wlr_ext_image_copy_capture_manager_v1_create(display, 1);
    wlr_ext_output_image_capture_source_manager_v1_create(display, 1);

    // fractional scale manager
    wlr_fractional_scale_manager =
        wlr_fractional_scale_manager_v1_create(display, 1);

    // alpha modifier
    wlr_alpha_modifier = wlr_alpha_modifier_v1_create(display);

    // single pixel buffer
    wlr_single_pixel_buffer_manager =
        wlr_single_pixel_buffer_manager_v1_create(display);

    // idle notifier
    wlr_idle_notifier = wlr_idle_notifier_v1_create(display);

    // avoid using "wayland-0" as display socket
    std::string socket;
    for (unsigned int i = 1; i <= 32; i++) {
        socket = "wayland-" + std::to_string(i);
        if (const int ret = wl_display_add_socket(display, socket.c_str());
            !ret)
            break;
        else
            wlr_log(WLR_ERROR,
                    "wl_display_add_socket for %s returned %d: skipping",
                    socket.c_str(), ret);
    }

    if (socket.empty()) {
        wlr_log(WLR_DEBUG, "%s", "Unable to open wayland socket");
        wlr_backend_destroy(backend);
        return;
    }

    // backend start
    if (!wlr_backend_start(backend)) {
        wlr_backend_destroy(backend);
        wl_display_destroy(display);
        ::exit(1);
    }

#ifdef XWAYLAND
    // init xwayland
    if ((xwayland = wlr_xwayland_create(display, compositor, true))) {
        // xwayland_ready
        xwayland_ready.notify = [](wl_listener *listener,
                                   [[maybe_unused]] void *data) {
            Server *server = wl_container_of(listener, server, xwayland_ready);

            // connect to server seat
            wlr_xwayland_set_seat(server->xwayland, server->seat);

            // set xcursor
            wlr_xcursor *xcursor = wlr_xcursor_manager_get_xcursor(
                server->cursor->cursor_mgr, "default", 1);
            if (xcursor) {
                wlr_xwayland_set_cursor(
                    server->xwayland, xcursor->images[0]->buffer,
                    xcursor->images[0]->width * 4, xcursor->images[0]->width,
                    xcursor->images[0]->height, xcursor->images[0]->hotspot_x,
                    xcursor->images[0]->hotspot_y);
            }
        };
        wl_signal_add(&xwayland->events.ready, &xwayland_ready);

        // new_xwayland_surface
        new_xwayland_surface.notify = [](wl_listener *listener, void *data) {
            Server *server =
                wl_container_of(listener, server, new_xwayland_surface);

            // toplevels are managed by workspaces
            new Toplevel(server, static_cast<wlr_xwayland_surface *>(data));
        };
        wl_signal_add(&xwayland->events.new_surface, &new_xwayland_surface);

        // don't connect to parent X11 server
        unsetenv("DISPLAY");

        // set new display
        setenv("DISPLAY", xwayland->display_name, 1);
        wlr_log(WLR_INFO, "started xwayland on $DISPLAY=%s",
                xwayland->display_name);
    } else
        wlr_log(WLR_ERROR, "%s", "failed to start xwayland");
#endif

    // start IPC
    if (config->ipc.enabled)
        ipc = new IPC(this);

    // set up signal handler
    sa.sa_handler = [](int sig) {
        if (sig == SIGCHLD)
            while (waitpid(-1, nullptr, WNOHANG) > 0)
                ;
        else if (sig == SIGINT || sig == SIGTERM)
            Server::get()->exit();
    };
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGPIPE, &sa, nullptr);

    // set wayland display to our socket
    setenv("WAYLAND_DISPLAY", socket.c_str(), true);

    // set xcursor paths
    char *xcursor_path = getenv("XCURSOR_PATH");
    std::string xcursor_path_str =
        xcursor_path == nullptr ? "" : ":" + std::string(xcursor_path);
    setenv("XCURSOR_PATH",
           ("/usr/share/icons:~/.local/share/icons" + xcursor_path_str).c_str(),
           true);

    // set envvars from config
    for (const auto &[key, value] : config->startup_env)
        setenv(key.c_str(), value.c_str(), true);

    // run startup commands from config
    for (const std::string &command : config->startup_commands)
        if (fork() == 0)
            execl("/bin/sh", "/bin/sh", "-c", command.c_str(), nullptr);

    // set systemd user environment
    const std::string systemd_user_env[] = {
        "systemctl --user set-environment XDG_CURRENT_DESKTOP=awm",
        "systemctl --user import-environment WAYLAND_DISPLAY DISPLAY "
        "XDG_CURRENT_DESKTOP AWM_SOCKET XCURSOR_SIZE XCURSOR_THEME",
        "dbus-update-activation-environment WAYLAND_DISPLAY DISPLAY "
        "XDG_CURRENT_DESKTOP AWM_SOCKET XCURSOR_SIZE XCURSOR_THEME"};
    for (const std::string &command : systemd_user_env)
        if (fork() == 0)
            execl("/bin/sh", "/bin/sh", "-c", command.c_str(), nullptr);

    // run thread for config updater
    config_thread = std::thread([&]() {
        while (running) {
            // update config
            config->update(this);

            // sleep
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });

    // run event loop
    wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s",
            socket.c_str());
    wl_display_run(display);
}

void Server::exit() const {
    wl_display_terminate(display);

    // run exit commands
    for (const std::string &command : config->exit_commands)
        if (fork() == 0)
            execl("/bin/sh", "/bin/sh", "-c", command.c_str(), nullptr);

    // stop IPC
    if (ipc)
        ipc->stop();
}

Server::~Server() {
    wl_display_destroy_clients(display);

    running = false;
    if (config_thread.joinable())
        config_thread.join();

    delete output_manager;

    wl_list_remove(&new_xdg_toplevel.link);

    wl_list_remove(&new_input.link);
    wl_list_remove(&request_cursor.link);
    wl_list_remove(&request_set_selection.link);
    wl_list_remove(&request_start_drag.link);
    wl_list_remove(&start_drag.link);

    wl_list_remove(&renderer_lost.link);

    wl_list_remove(&new_shell_surface.link);
    wl_list_remove(&new_session_lock.link);
    wl_list_remove(&new_virtual_pointer.link);
    wl_list_remove(&new_virtual_keyboard.link);
    wl_list_remove(&new_pointer_constraint.link);
    wl_list_remove(&xdg_activation_activate.link);

    LayerSurface *surface, *tmp;
    wl_list_for_each_safe(surface, tmp, &layer_surfaces, link) delete surface;

#ifdef XWAYLAND
    wl_list_remove(&xwayland_ready.link);
    wl_list_remove(&new_xwayland_surface.link);
    wlr_xwayland_destroy(xwayland);
#endif

    wlr_scene_node_destroy(&scene->tree.node);

    delete cursor;

    wlr_allocator_destroy(allocator);
    wlr_renderer_destroy(renderer);
    wlr_backend_destroy(backend);
    wl_display_destroy(display);
}
