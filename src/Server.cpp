#include "Server.h"
#include "IdleInhibitor.h"
#include "Keyboard.h"
#include "SessionLock.h"
#include "TearingController.h"
#include "wlr.h"
#include <sys/signalfd.h>

// get workspace by toplevel
Workspace *Server::get_workspace(Toplevel *toplevel) const {
    return workspace_manager->get_workspace_for_toplevel(toplevel);
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
    if (wlr_xdg_surface *xdg_surface =
            wlr_xdg_surface_try_from_wlr_surface(surface))
        return static_cast<Toplevel *>(xdg_surface->data);

#ifdef XWAYLAND
    if (wlr_xwayland_surface *xsurface =
            wlr_xwayland_surface_try_from_wlr_surface(surface))
        return static_cast<Toplevel *>(xsurface->data);
#endif

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

    // use bind exit as exit shortcuts inhibitor
    if (active_keyboard_shortcuts_inhibitor) {
        if (bind.name == BIND_EXIT) {
            wlr_keyboard_shortcuts_inhibitor_v1_deactivate(
                active_keyboard_shortcuts_inhibitor);
            return true;
        }
        return false;
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
    case BIND_WINDOW_RESIZE:
        // resize the active toplevel with the mouse
        if (Toplevel *active = output->get_active()->active_toplevel) {
            // find what edges to resize based on cursor position
            double x = cursor->cursor->x, y = cursor->cursor->y;
            wlr_box geo = active->geometry;

            uint32_t edges = 0;
            if (x < geo.x + (double)geo.width / 2)
                edges |= WLR_EDGE_LEFT;
            if (x > geo.x + (double)geo.width / 2)
                edges |= WLR_EDGE_RIGHT;
            if (y < geo.y + (double)geo.height / 2)
                edges |= WLR_EDGE_TOP;
            if (y > geo.y + (double)geo.height / 2)
                edges |= WLR_EDGE_BOTTOM;

            active->begin_interactive(CURSORMODE_RESIZE, edges);
        }
        break;
    case BIND_WINDOW_PIN:
        // pin the active toplevel
        if (Toplevel *active = output->get_active()->active_toplevel)
            active->pinned = !active->pinned;
        break;
    case BIND_WINDOW_UP:
    case BIND_WINDOW_DOWN:
    case BIND_WINDOW_LEFT:
    case BIND_WINDOW_RIGHT:
        // focus the toplevel in the specified direction
        if (Toplevel *other = output->get_active()->in_direction(
                static_cast<wlr_direction>(1 << (bind.name - BIND_WINDOW_UP))))
            output->get_active()->focus_toplevel(other);
        break;
    case BIND_WINDOW_CLOSE:
        // close the active toplevel
        output->get_active()->close_active();
        break;
    case BIND_WINDOW_SWAP_UP:
    case BIND_WINDOW_SWAP_DOWN:
    case BIND_WINDOW_SWAP_LEFT:
    case BIND_WINDOW_SWAP_RIGHT:
        // swap the active toplevel with the one in specified direction
        if (Toplevel *other =
                output->get_active()->in_direction(static_cast<wlr_direction>(
                    1 << (bind.name - BIND_WINDOW_SWAP_UP))))
            output->get_active()->swap(other);
        break;
    case BIND_WINDOW_HALF_UP:
    case BIND_WINDOW_HALF_DOWN:
    case BIND_WINDOW_HALF_LEFT:
    case BIND_WINDOW_HALF_RIGHT: {
        // set the active toplevel to take up the specified half of the screen
        Workspace *workspace = output->get_active();
        if (Toplevel *active = workspace->active_toplevel)
            workspace->set_half_in_direction(
                active, static_cast<wlr_direction>(
                            1 << (bind.name - BIND_WINDOW_HALF_UP)));
        break;
    }
    case BIND_WORKSPACE_TILE:
        // tile all toplevels in workspace
        output->get_active()->tile();
        break;
    case BIND_WORKSPACE_TILE_SANS:
        // tile all toplevels in workspace sans active
        output->get_active()->tile_sans_active();
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

        return current->move_to(current->active_toplevel, target);
    }
    case BIND_NONE:
    default: {
        // handle user-defined binds
        for (const auto &[cmd_bind, cmd] : config->commands)
            if (cmd_bind == bind) {
                spawn(cmd);
                return true;
            }

        return false;
    }
    }

    return true;
}

void Server::update_idle_inhibitors(wlr_surface *sans) {
    bool inhibited = false;

    // check if any inhibitor is active
    wlr_idle_inhibitor_v1 *inhibitor;
    wl_list_for_each(inhibitor, &wlr_idle_inhibit_manager->inhibitors, link) {
        wlr_surface *surface = wlr_surface_get_root_surface(inhibitor->surface);
        wlr_scene_tree *tree = static_cast<wlr_scene_tree *>(surface->data);

        // set inhibited if active
        int _unused_x, _unused_y;
        if (sans != surface &&
            (!tree ||
             wlr_scene_node_coords(&tree->node, &_unused_x, &_unused_y))) {
            inhibited = true;
            break;
        }
    }

    // set notifier status
    wlr_idle_notifier_v1_set_inhibited(wlr_idle_notifier, inhibited);
}

// run a command with sh
void Server::spawn(const std::string &command) const {
    if (fork() == 0)
        execl("/bin/sh", "/bin/sh", "-c", command.c_str(), nullptr);
}

static void recreate_renderer(void *data) {
    Server *server = static_cast<Server *>(data);
    server->recreating_renderer = nullptr;

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
    wl_list_for_each_safe(output, tmp, &server->output_manager->outputs, link)
        wlr_output_init_render(output->wlr_output, server->allocator,
                               server->renderer);

    // destroy old renderer and allocator
    wlr_allocator_destroy(old_allocator);
    wlr_renderer_destroy(old_renderer);
}

Server::Server(Config *config) : config(config) {
    // set envvars from config
    for (const auto &[key, value] : config->startup_env)
        setenv(key.c_str(), value.c_str(), true);

    // display
    display = wl_display_create();

    // event loop
    event_loop = wl_display_get_event_loop(display);

    // backend
    if (!(backend = wlr_backend_autocreate(event_loop, &session))) {
        wlr_log(WLR_ERROR, "%s", "failed to create wlr_backend");
        ::exit(1);
    }

    // renderer
    if (!(renderer = wlr_renderer_autocreate(backend))) {
        wlr_log(WLR_ERROR, "%s", "failed to create wlr_renderer");
        ::exit(1);
    }

    // renderer_lost
    renderer_lost.notify = [](wl_listener *listener,
                              [[maybe_unused]] void *data) {
        // renderer recovery (thanks sway)
        Server *server = wl_container_of(listener, server, renderer_lost);

        if (server->recreating_renderer) {
            wlr_log(WLR_INFO, "%s", "renderer already being recreated");
            return;
        }

        wlr_log(WLR_INFO, "%s", "re-creating renderer after GPU reset");
        server->recreating_renderer = wl_event_loop_add_idle(
            server->event_loop, recreate_renderer, server);
    };
    wl_signal_add(&renderer->events.lost, &renderer_lost);

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
    if (!(allocator = wlr_allocator_autocreate(backend, renderer))) {
        wlr_log(WLR_ERROR, "%s", "failed to create wlr_allocator");
        ::exit(1);
    }

    // wlr compositor
    compositor = wlr_compositor_create(display, 6, renderer);
    wlr_subcompositor_create(display);
    wlr_data_device_manager_create(display);

    // primary selection (early init to fix some gtk bs)
    wlr_primary_selection_v1_device_manager_create(display);

    // output manager
    output_manager = new OutputManager(this);

    // workspace manager
    workspace_manager = new WorkspaceManager(this);

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

    // session lock
    wlr_session_lock_manager = wlr_session_lock_manager_v1_create(display);

    new_session_lock.notify = [](wl_listener *listener, void *data) {
        Server *server = wl_container_of(listener, server, new_session_lock);
        auto session_lock = static_cast<wlr_session_lock_v1 *>(data);

        wlr_scene_node_set_enabled(&server->lock_background->node, true);

        if (server->current_session_lock) {
            wlr_session_lock_v1_destroy(session_lock);
            return;
        }

        new SessionLock(server, session_lock);
    };
    wl_signal_add(&wlr_session_lock_manager->events.new_lock,
                  &new_session_lock);

    // lock background
    const wlr_box full_box = output_manager->full_geometry();
    const float color[] = {0.1f, 0.1f, 0.1f, 1.0f};
    lock_background = wlr_scene_rect_create(layers.lock, full_box.width,
                                            full_box.height, color);
    wlr_scene_node_set_enabled(&lock_background->node, false);

    // relative pointer
    wlr_relative_pointer_manager =
        wlr_relative_pointer_manager_v1_create(display);

    // seat
    seat = new Seat(this);

    // cursor
    cursor = new Cursor(seat);

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

    // keyboards
    wl_list_init(&keyboards);

    // xdg activation
    wlr_xdg_activation = wlr_xdg_activation_v1_create(display);

    xdg_activation_activate.notify = [](wl_listener *listener, void *data) {
        Server *server =
            wl_container_of(listener, server, xdg_activation_activate);
        const auto event =
            static_cast<wlr_xdg_activation_v1_request_activate_event *>(data);

        // get toplevel associated with surface
        Toplevel *toplevel = server->get_toplevel(event->surface);
        if (!toplevel)
            return;
        if (!toplevel->xdg_toplevel)
            return;
        if (!toplevel->xdg_toplevel->base->surface->mapped)
            return;

        // get the workspace and output that contain the toplevel
        Workspace *workspace = server->get_workspace(toplevel);
        Output *output = workspace->output;

        // focus on window activate
        switch (server->config->general.fowa) {
        case FOWA_ANY: {
            // set workspace to toplevel's workspace
            if (output->get_active() != workspace)
                output->set_workspace(workspace);

            // focus toplevel
            toplevel->focus();
            break;
        }
        case FOWA_ACTIVE: {
            // only focus toplevel if in the active workspace
            if (output->get_active()->contains(toplevel))
                toplevel->focus();

            break;
        }
        default:
            break;
        }
    };
    wl_signal_add(&wlr_xdg_activation->events.request_activate,
                  &xdg_activation_activate);

    // xdg dialog
    wlr_xdg_wm_dialog = wlr_xdg_wm_dialog_v1_create(display, 1);

    new_xdg_dialog.notify = [](wl_listener *listener, void *data) {
        Server *server = wl_container_of(listener, server, new_xdg_dialog);
        wlr_xdg_dialog_v1 *dialog = static_cast<wlr_xdg_dialog_v1 *>(data);

        // get associated toplevel
        Toplevel *toplevel =
            static_cast<Toplevel *>(dialog->xdg_toplevel->base->data);

        // a toplevel should only have one dialog, technically I should raise
        // error already_used but I'm unsure how to do this with wlroots
        if (toplevel->wlr_xdg_dialog)
            return;

        // set dialog
        toplevel->wlr_xdg_dialog = dialog;

        // set destroy listener
        toplevel->xdg_dialog_destroy.notify = [](wl_listener *listener,
                                                 [[maybe_unused]] void *data) {
            Toplevel *toplevel =
                wl_container_of(listener, toplevel, xdg_dialog_destroy);
            toplevel->wlr_xdg_dialog = nullptr;
            wl_list_remove(&toplevel->xdg_dialog_destroy.link);
        };
        wl_signal_add(&dialog->events.destroy, &toplevel->xdg_dialog_destroy);
    };
    wl_signal_add(&wlr_xdg_wm_dialog->events.new_dialog, &new_xdg_dialog);

    // system bell
    wlr_xdg_system_bell = wlr_xdg_system_bell_v1_create(display, 1);

    ring_system_bell.notify = [](wl_listener *listener, void *data) {
        Server *server = wl_container_of(listener, server, ring_system_bell);
        const auto event =
            static_cast<wlr_xdg_system_bell_v1_ring_event *>(data);

        if (!event->client) {
            wlr_log(WLR_ERROR, "%s", "system bell client is NULL");
            return;
        }

        // get bell command defined in config
        const std::string &command = server->config->general.system_bell;

        // spawn system bell command if provided
        if (!command.empty())
            server->spawn(command);
    };
    wl_signal_add(&wlr_xdg_system_bell->events.ring, &ring_system_bell);

    // keyboard shortcuts inhibitor
    wlr_keyboard_shortcuts_inhibit_manager =
        wlr_keyboard_shortcuts_inhibit_v1_create(display);

    new_keyboard_shortcuts_inhibit.notify = [](wl_listener *listener,
                                               void *data) {
        Server *server =
            wl_container_of(listener, server, new_keyboard_shortcuts_inhibit);
        const auto inhibit =
            static_cast<wlr_keyboard_shortcuts_inhibitor_v1 *>(data);

        // do not activate if already active
        if (server->active_keyboard_shortcuts_inhibitor)
            return;

        // set up destroy listener
        server->destroy_keyboard_shortcuts_inhibitor.notify =
            [](wl_listener *listener, [[maybe_unused]] void *data) {
                Server *server = wl_container_of(
                    listener, server, destroy_keyboard_shortcuts_inhibitor);

                // deactivate
                wlr_keyboard_shortcuts_inhibitor_v1_deactivate(
                    server->active_keyboard_shortcuts_inhibitor);

                // remove destroy listener
                wl_list_remove(
                    &server->destroy_keyboard_shortcuts_inhibitor.link);

                // clear pointer
                server->active_keyboard_shortcuts_inhibitor = nullptr;
            };
        wl_signal_add(&inhibit->events.destroy,
                      &server->destroy_keyboard_shortcuts_inhibitor);

        // activate shortcuts inhibitor
        server->active_keyboard_shortcuts_inhibitor = inhibit;
        wlr_keyboard_shortcuts_inhibitor_v1_activate(inhibit);
    };
    wl_signal_add(&wlr_keyboard_shortcuts_inhibit_manager->events.new_inhibitor,
                  &new_keyboard_shortcuts_inhibit);

    // drm lease
    if ((wlr_drm_lease_manager =
             wlr_drm_lease_v1_manager_create(display, backend))) {
        drm_lease_request.notify = [](wl_listener *listener, void *data) {
            Server *server =
                wl_container_of(listener, server, drm_lease_request);
            auto request = static_cast<wlr_drm_lease_request_v1 *>(data);

            if (!request) {
                wlr_drm_lease_request_v1_reject(request);
                return;
            }

            for (size_t i = 0; i != request->n_connectors; ++i) {
                auto output =
                    static_cast<Output *>(request->connectors[i]->output->data);
                if (!output)
                    continue;

                wlr_output_layout_remove(server->output_manager->layout,
                                         output->wlr_output);
                output->scene_output = nullptr;
            }
        };
        wl_signal_add(&wlr_drm_lease_manager->events.request,
                      &drm_lease_request);
    }

    // idle inhibitor
    wlr_idle_inhibit_manager = wlr_idle_inhibit_v1_create(display);

    new_idle_inhibitor.notify = [](wl_listener *listener, void *data) {
        Server *server = wl_container_of(listener, server, new_idle_inhibitor);
        wlr_idle_inhibitor_v1 *inhibitor =
            static_cast<wlr_idle_inhibitor_v1 *>(data);

        new IdleInhibitor(inhibitor, server);
    };
    wl_signal_add(&wlr_idle_inhibit_manager->events.new_inhibitor,
                  &new_idle_inhibitor);

    // xdg decoration
#ifdef XDG_DECORATION
    wl_list_init(&decorations);
    xdg_decoration_manager = wlr_xdg_decoration_manager_v1_create(display);

    new_xdg_decoration.notify = [](wl_listener *listener, void *data) {
        Server *server = wl_container_of(listener, server, new_xdg_decoration);
        wlr_xdg_toplevel_decoration_v1 *decoration =
            static_cast<wlr_xdg_toplevel_decoration_v1 *>(data);

        // must have valid surface
        wlr_xdg_surface *surface = decoration->toplevel->base;
        if (!surface || !surface->data) {
            wlr_log(WLR_ERROR, "%s", "invalid surface for xdg decoration");
            return;
        }

        // create decoration
        // new Decoration(server, decoration);
    };
    wl_signal_add(&xdg_decoration_manager->events.new_toplevel_decoration,
                  &new_xdg_decoration);
#endif

    // foreign toplevel image capture source
    wlr_ext_foreign_toplevel_image_capture_source_manager =
        wlr_ext_foreign_toplevel_image_capture_source_manager_v1_create(display,
                                                                        1);

    new_toplevel_capture_request.notify = [](wl_listener *listener,
                                             void *data) {
        Server *server =
            wl_container_of(listener, server, new_toplevel_capture_request);
        auto request = static_cast<
            wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request *>(
            data);
        Toplevel *toplevel =
            static_cast<Toplevel *>(request->toplevel_handle->data);

        if (!toplevel->image_capture_source) {
            // no image capture source, create one
            toplevel->image_capture_source =
                wlr_ext_image_capture_source_v1_create_with_scene_node(
                    &toplevel->image_capture->tree.node, server->event_loop,
                    server->allocator, server->renderer);

            if (!toplevel->image_capture_source)
                return;
        }

        wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request_accept(
            request, toplevel->image_capture_source);
    };
    wl_signal_add(&wlr_ext_foreign_toplevel_image_capture_source_manager->events
                       .new_request,
                  &new_toplevel_capture_request);

    // toplevel tag
    wlr_xdg_toplevel_tag_manager =
        wlr_xdg_toplevel_tag_manager_v1_create(display, 1);

    xdg_toplevel_set_tag.notify = [](wl_listener *listener, void *data) {
        Server *server =
            wl_container_of(listener, server, xdg_toplevel_set_tag);
        const auto event =
            static_cast<wlr_xdg_toplevel_tag_manager_v1_set_tag_event *>(data);

        Toplevel *toplevel =
            static_cast<Toplevel *>(event->toplevel->base->data);
        toplevel->tag = event->tag;
    };
    wl_signal_add(&wlr_xdg_toplevel_tag_manager->events.set_tag,
                  &xdg_toplevel_set_tag);

    // tearing control
    wlr_tearing_control_manager =
        wlr_tearing_control_manager_v1_create(display, 1);
    wl_list_init(&tearing_controllers);

    new_tearing_control.notify = [](wl_listener *listener, void *data) {
        Server *server = wl_container_of(listener, server, new_tearing_control);
        wlr_tearing_control_v1 *tearing_control =
            static_cast<wlr_tearing_control_v1 *>(data);

        new TearingController(server, tearing_control);
    };
    wl_signal_add(&wlr_tearing_control_manager->events.new_object,
                  &new_tearing_control);

    // color manager
    if (renderer->features.input_color_transform) {
        const struct wlr_color_manager_v1_features color_manager_features = {
            false, true, false, false, false, true, false, false};
        const enum wp_color_manager_v1_render_intent
            color_manager_render_intents[] = {
                WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL,
            };
        const enum wp_color_manager_v1_transfer_function
            color_manager_transfer_functions[] = {
                WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB,
                WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ,
                WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR,
            };
        const enum wp_color_manager_v1_primaries color_manager_primaries[] = {
            WP_COLOR_MANAGER_V1_PRIMARIES_SRGB,
            WP_COLOR_MANAGER_V1_PRIMARIES_BT2020,
        };
        const wlr_color_manager_v1_options color_manager_options = {
            color_manager_features,
            color_manager_render_intents,
            sizeof(color_manager_render_intents) /
                sizeof(color_manager_render_intents[0]),
            color_manager_transfer_functions,
            sizeof(color_manager_transfer_functions) /
                sizeof(color_manager_transfer_functions[0]),
            color_manager_primaries,
            sizeof(color_manager_primaries) /
                sizeof(color_manager_primaries[0]),
        };
        wlr_color_manager_v1_create(display, 1, &color_manager_options);
    }

    // color representation
    wp_color_representation_surface_v1_alpha_mode
        color_representation_alpha_modes[] = {
            WP_COLOR_REPRESENTATION_SURFACE_V1_ALPHA_MODE_STRAIGHT};
    const wlr_color_representation_v1_coeffs_and_range
        color_representation_coeffs_and_range[] = {
            wlr_color_representation_v1_coeffs_and_range{
                WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_IDENTITY,
                WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_FULL}};
    const wlr_color_representation_v1_options color_representation_options = {
        color_representation_alpha_modes,
        sizeof(color_representation_alpha_modes) /
            sizeof(color_representation_alpha_modes[0]),
        color_representation_coeffs_and_range,
        sizeof(color_representation_coeffs_and_range) /
            sizeof(color_representation_coeffs_and_range[0]),
    };
    wlr_color_representation_manager_v1_create(display, 1,
                                               &color_representation_options);

    // xdg foreign
    wlr_xdg_foreign_registry *xdg_foreign_registry =
        wlr_xdg_foreign_registry_create(display);
    wlr_xdg_foreign_v1_create(display, xdg_foreign_registry);
    wlr_xdg_foreign_v2_create(display, xdg_foreign_registry);

    // foreign toplevel list
    wlr_foreign_toplevel_list =
        wlr_ext_foreign_toplevel_list_v1_create(display, 1);

    // foreign toplevel manager
    wlr_foreign_toplevel_manager =
        wlr_foreign_toplevel_manager_v1_create(display);

    // input method
    wlr_input_method_manager = wlr_input_method_manager_v2_create(display);

    // text input manager
    wlr_text_input_manager = wlr_text_input_manager_v3_create(display);

    // idle notifier
    wlr_idle_notifier = wlr_idle_notifier_v1_create(display);

    // content type
    wlr_content_type_manager = wlr_content_type_manager_v1_create(display, 1);

    // viewporter
    wlr_viewporter_create(display);

    // presentation
    wlr_presentation_create(display, backend, 2);

    // export dmabuf manager
    wlr_export_dmabuf_manager_v1_create(display);

    // screencopy manager
    wlr_screencopy_manager_v1_create(display);

    // ext data control manager
    wlr_ext_data_control_manager_v1_create(display, 1);

    // gamma control manager
    wlr_scene_set_gamma_control_manager_v1(
        scene, wlr_gamma_control_manager_v1_create(display));

    // image copy capture manager
    wlr_ext_image_copy_capture_manager_v1_create(display, 1);
    wlr_ext_output_image_capture_source_manager_v1_create(display, 1);

    // fractional scale manager
    wlr_fractional_scale_manager_v1_create(display, 1);

    // alpha modifier
    wlr_alpha_modifier_v1_create(display);

    // single pixel buffer
    wlr_single_pixel_buffer_manager_v1_create(display);

    // fixes
    wlr_fixes_create(display, 1);

#ifdef XWAYLAND
    // init xwayland
    if ((xwayland = wlr_xwayland_create(display, compositor, true))) {
        // xwayland_ready
        xwayland_ready.notify = [](wl_listener *listener,
                                   [[maybe_unused]] void *data) {
            Server *server = wl_container_of(listener, server, xwayland_ready);

            // connect to server seat
            wlr_xwayland_set_seat(server->xwayland, server->seat->wlr_seat);

            // set xcursor
            wlr_xcursor *xcursor = wlr_xcursor_manager_get_xcursor(
                server->cursor->xcursor_manager, "default", 1);
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

        // set new display
        setenv("DISPLAY", xwayland->display_name, 1);
        wlr_log(WLR_INFO, "started xwayland on $DISPLAY=%s",
                xwayland->display_name);
    } else {
        wlr_log(WLR_ERROR, "%s", "failed to start xwayland");
        unsetenv("DISPLAY");
    }
#endif

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

    // headless backend
    headless_backend = wlr_headless_backend_create(event_loop);
    if (headless_backend)
        wlr_multi_backend_add(backend, headless_backend);
    else {
        wlr_backend_destroy(backend);
        ::exit(1);
    }

    // backend start
    if (!wlr_backend_start(backend)) {
        wlr_log(WLR_ERROR, "%s", "Failed to start backend");
        wlr_backend_destroy(backend);
        ::exit(1);
    }

    // start IPC
    if (config->ipc.enabled)
        ipc = new IPC(this, config->ipc.path);

    // create signalfd
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);

    // block signals for all threads
    sigprocmask(SIG_BLOCK, &mask, nullptr);

    int sfd = signalfd(-1, &mask, SFD_CLOEXEC);
    signal_handler = wl_event_loop_add_fd(
        event_loop, sfd, WL_EVENT_READABLE,
        []([[maybe_unused]] int fd, uint32_t mask, void *data) {
            if (!(mask & WL_EVENT_READABLE))
                return 0;

            struct signalfd_siginfo fdsi;
            ssize_t s = read(fd, &fdsi, sizeof(fdsi));
            if (s != sizeof(fdsi))
                return 0;

            Server *server = static_cast<Server *>(data);
            switch (fdsi.ssi_signo) {
            case SIGCHLD:
                while (waitpid(-1, nullptr, WNOHANG) > 0)
                    ;
                break;
            case SIGINT:
            case SIGTERM:
                server->exit();
                break;
            default:
                break;
            }
            return 0;
        },
        this);

    // set stdout to non-blocking
    if (int flags = fcntl(STDOUT_FILENO, F_GETFL);
        flags < 0 || fcntl(STDOUT_FILENO, F_SETFL, flags | O_NONBLOCK) < 0)
        close(STDOUT_FILENO);

    // set wayland display to our socket
    setenv("WAYLAND_DISPLAY", socket.c_str(), true);

    // set xcursor paths
    char *xcursor_path = getenv("XCURSOR_PATH");
    std::string xcursor_path_str =
        xcursor_path == nullptr ? "" : ":" + std::string(xcursor_path);
    setenv("XCURSOR_PATH",
           ("/usr/share/icons:~/.local/share/icons" + xcursor_path_str).c_str(),
           true);

    // run startup commands from config
    for (const std::string &command : config->startup_commands)
        spawn(command);

    // set systemd user environment
    const std::string systemd_user_env[] = {
#ifdef SYSTEMD
        "systemctl --user set-environment XDG_CURRENT_DESKTOP=awm",
        "systemctl --user import-environment WAYLAND_DISPLAY DISPLAY "
        "XDG_CURRENT_DESKTOP AWM_SOCKET XCURSOR_SIZE XCURSOR_THEME",
#endif
        "dbus-update-activation-environment WAYLAND_DISPLAY DISPLAY "
        "XDG_CURRENT_DESKTOP AWM_SOCKET XCURSOR_SIZE XCURSOR_THEME"};
    for (const std::string &command : systemd_user_env)
        spawn(command);

    // add config updater to event loop
    config_update_timer = wl_event_loop_add_timer(
        event_loop,
        [](void *data) {
            Server *server = static_cast<Server *>(data);

            server->config->update(server);

            wl_event_source_timer_update(server->config_update_timer, 1000);
            return 0;
        },
        this);
    wl_event_source_timer_update(config_update_timer, 1000);

    // run event loop
    wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s",
            socket.c_str());
    wl_display_run(display);
}

// stop server and run exit commands
void Server::exit() {
    wl_display_terminate(display);

    // run exit commands
    for (const std::string &command : config->exit_commands)
        spawn(command);

    // stop IPC
    if (ipc) {
        ipc->stop();
        ipc = nullptr;
    }
}

Server::~Server() {
    wl_list_remove(&renderer_lost.link);

    wl_list_remove(&new_xdg_toplevel.link);
    wl_list_remove(&new_shell_surface.link);
    wl_list_remove(&new_session_lock.link);
    wl_list_remove(&new_pointer_constraint.link);

    wl_list_remove(&xdg_activation_activate.link);
    wl_list_remove(&new_xdg_dialog.link);
    wl_list_remove(&new_keyboard_shortcuts_inhibit.link);
    wl_list_remove(&ring_system_bell.link);

    if (wlr_drm_lease_manager)
        wl_list_remove(&drm_lease_request.link);

    wl_list_remove(&new_idle_inhibitor.link);
    wl_list_remove(&new_toplevel_capture_request.link);
    wl_list_remove(&xdg_toplevel_set_tag.link);

    TearingController *tc, *tct;
    wl_list_for_each_safe(tc, tct, &tearing_controllers, link) delete tc;
    wl_list_remove(&new_tearing_control.link);

#ifdef XDG_DECORATION
    wl_list_remove(&new_xdg_decoration.link);
#endif

#ifdef XWAYLAND
    wl_list_remove(&xwayland_ready.link);
    wl_list_remove(&new_xwayland_surface.link);
    wlr_xwayland_destroy(xwayland);
#endif

    delete output_manager;
    delete workspace_manager;
    delete seat;
    delete cursor;

    wl_display_destroy_clients(display);

    Keyboard *kb, *kbt;
    wl_list_for_each_safe(kb, kbt, &keyboards, link) delete kb;

    LayerSurface *ls, *lst;
    wl_list_for_each_safe(ls, lst, &layer_surfaces, link) delete ls;

    wl_event_source_remove(signal_handler);
    wl_event_source_remove(config_update_timer);

    wlr_allocator_destroy(allocator);
    wlr_renderer_destroy(renderer);
    wlr_backend_destroy(backend);
    wl_display_destroy(display);

    wlr_scene_node_destroy(&scene->tree.node);
}
