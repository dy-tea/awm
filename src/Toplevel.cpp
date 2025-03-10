#include "Server.h"

void Toplevel::map_notify(wl_listener *listener, [[maybe_unused]] void *data) {
    // on map or display
    Toplevel *toplevel = wl_container_of(listener, toplevel, map);

    // xdg toplevel
    if (const wlr_xdg_toplevel *xdg_toplevel = toplevel->xdg_toplevel) {

        // get the focused output
        if (Output *output = toplevel->server->focused_output()) {
            // set the fractional scale for this surface
            const float scale = output->wlr_output->scale;
            wlr_fractional_scale_v1_notify_scale(xdg_toplevel->base->surface,
                                                 scale);
            wlr_surface_set_preferred_buffer_scale(xdg_toplevel->base->surface,
                                                   ceil(scale));

            // get usable area of the output
            wlr_box usable_area = output->usable_area;

            // get scheduled width and height
            uint32_t width = xdg_toplevel->scheduled.width > 0
                                 ? xdg_toplevel->scheduled.width
                                 : xdg_toplevel->current.width;
            uint32_t height = xdg_toplevel->scheduled.height > 0
                                  ? xdg_toplevel->scheduled.height
                                  : xdg_toplevel->current.height;

            // set current width and height if not scheduled
            if (!width || !height) {
                width = xdg_toplevel->base->surface->current.width;
                height = xdg_toplevel->base->surface->current.height;
            }

            // ensure size does not exceed output
            width = std::min(width, static_cast<uint32_t>(usable_area.width));
            height =
                std::min(height, static_cast<uint32_t>(usable_area.height));

            wlr_box output_box = output->layout_geometry;

            int32_t x = output_box.x + (usable_area.width - width) / 2;
            int32_t y = output_box.y + (usable_area.height - height) / 2;

            // ensure position falls in bounds
            x = std::max(x, usable_area.x);
            y = std::max(y, usable_area.y);

            // set the position
            wlr_scene_node_set_position(&toplevel->scene_tree->node, x, y);

            // save geometry
            toplevel->geometry.width = width;
            toplevel->geometry.height = height;
            toplevel->geometry.x = x;
            toplevel->geometry.y = y;

            // add toplevel to active workspace and focus it
            output->get_active()->add_toplevel(toplevel, true);
        }
    }
#ifdef XWAYLAND
    else {
        // xwayland surface

        if (Output *output = toplevel->server->focused_output()) {
            // create scene surface
            toplevel->scene_tree =
                wlr_scene_tree_create(toplevel->server->layers.floating);
            toplevel->scene_surface = wlr_scene_surface_create(
                toplevel->scene_tree, toplevel->xwayland_surface->surface);

            // get usable area of the output
            wlr_box area = output->usable_area;

            // calcualte the width and height
            int width = toplevel->xwayland_surface->width;
            int height = toplevel->xwayland_surface->height;

            // ensure size does not exceed output
            if (width > area.width)
                width = area.width;

            if (height > area.height)
                height = area.height;

            // get surface position
            int x = toplevel->xwayland_surface->x;
            int y = toplevel->xwayland_surface->y;

            // center the surface to the focused output if zero
            if (!x)
                x += area.x + (output->layout_geometry.width - width) / 2;
            if (!y)
                y += area.y + (output->layout_geometry.height - height) / 2;

            // send a configure event
            wlr_xwayland_surface_configure(toplevel->xwayland_surface, x, y,
                                           width, height);

            // set the position
            wlr_scene_node_set_position(&toplevel->scene_surface->buffer->node,
                                        x, y);

            // add the toplevel to the scene tree
            toplevel->scene_surface->buffer->node.data = toplevel;
            toplevel->scene_tree->node.data = toplevel;

            // xwayland commit
            toplevel->xwayland_commit.notify = [](wl_listener *listener,
                                                  [[maybe_unused]] void *data) {
                Toplevel *toplevel =
                    wl_container_of(listener, toplevel, xwayland_commit);
                const wlr_surface_state *state =
                    &toplevel->xwayland_surface->surface->current;

                wlr_box new_box{
                    0,
                    0,
                    state->width,
                    state->height,
                };

                if (new_box.width != toplevel->saved_geometry.width ||
                    new_box.height != toplevel->saved_geometry.height)
                    memcpy(&toplevel->saved_geometry, &new_box,
                           sizeof(wlr_box));
            };
            wl_signal_add(&toplevel->xwayland_surface->surface->events.commit,
                          &toplevel->xwayland_commit);

            // set seat if requested
            if (wlr_xwayland_surface_override_redirect_wants_focus(
                    toplevel->xwayland_surface))
                wlr_xwayland_set_seat(toplevel->server->xwayland,
                                      toplevel->server->seat);

            // add to active workspace
            output->get_active()->add_toplevel(toplevel, true);
        }
    }
#endif

    // notify clients
    if (toplevel->server->ipc) {
        toplevel->server->ipc->notify_clients(IPC_TOPLEVEL_LIST);
        toplevel->server->ipc->notify_clients(IPC_WORKSPACE_LIST);
    }
}

void Toplevel::unmap_notify(wl_listener *listener,
                            [[maybe_unused]] void *data) {
    Toplevel *toplevel = wl_container_of(listener, toplevel, unmap);

    Server *server = toplevel->server;

    // deactivate
    if (toplevel == server->grabbed_toplevel)
        server->cursor->reset_mode();

    // remove from workspace
    if (Workspace *workspace = server->get_workspace(toplevel))
        workspace->close(toplevel);

    // remove link
    wl_list_remove(&toplevel->link);

    // notify clients
    if (IPC *ipc = server->ipc) {
        ipc->notify_clients(IPC_TOPLEVEL_LIST);
        ipc->notify_clients(IPC_WORKSPACE_LIST);
    }
}

// create a foreign toplevel handle
void Toplevel::create_handle() {
    // set foreign toplevel initial state
    handle = wlr_foreign_toplevel_handle_v1_create(
        server->wlr_foreign_toplevel_manager);

    if (xdg_toplevel) {
        if (xdg_toplevel->title)
            wlr_foreign_toplevel_handle_v1_set_title(handle,
                                                     xdg_toplevel->title);

        if (xdg_toplevel->app_id)
            wlr_foreign_toplevel_handle_v1_set_app_id(handle,
                                                      xdg_toplevel->app_id);
    }
#ifdef XWAYLAND
    else if (xwayland_surface) {
        if (xwayland_surface->title)
            wlr_foreign_toplevel_handle_v1_set_title(handle,
                                                     xwayland_surface->title);

        if (xwayland_surface->class_)
            wlr_foreign_toplevel_handle_v1_set_app_id(handle,
                                                      xwayland_surface->class_);
    }
#endif

    // handle_request_maximize
    handle_request_maximize.notify = [](wl_listener *listener, void *data) {
        Toplevel *toplevel =
            wl_container_of(listener, toplevel, handle_request_maximize);

        const auto *event =
            static_cast<wlr_foreign_toplevel_handle_v1_maximized_event *>(data);

        if (toplevel->maximized() != event->maximized)
            toplevel->set_maximized(event->maximized);
        toplevel->update_foreign_toplevel();
    };
    wl_signal_add(&handle->events.request_maximize, &handle_request_maximize);

    // handle_request_minimize
    handle_request_minimize.notify = [](wl_listener *listener,
                                        [[maybe_unused]] void *data) {
        Toplevel *toplevel =
            wl_container_of(listener, toplevel, handle_request_minimize);

        // wlr_foreign_toplevel_handle_v1_minimized_event *event =
        //     static_cast<wlr_foreign_toplevel_handle_v1_minimized_event
        //     *>(data);
        //
        //  FIXME: since awm doesn't support minimization, just update the
        //  toplevel

        notify_send(
            "%s",
            "Minimizing foreign toplevels is not supported, expect issues");
        wlr_scene_node_lower_to_bottom(&toplevel->scene_tree->node);

        toplevel->update_foreign_toplevel();
    };
    wl_signal_add(&handle->events.request_minimize, &handle_request_minimize);

    // handle_request_fullscreen
    handle_request_fullscreen.notify = [](wl_listener *listener, void *data) {
        Toplevel *toplevel =
            wl_container_of(listener, toplevel, handle_request_fullscreen);

        const auto *event =
            static_cast<wlr_foreign_toplevel_handle_v1_fullscreen_event *>(
                data);

        // output specified
        if (event->output) {
            // get the source and target workspaces
            Workspace *target =
                toplevel->server->get_output(event->output)->get_active();
            Workspace *source = toplevel->server->get_workspace(toplevel);

            // move the toplevel to the target workspace
            source->move_to(toplevel, target);
        }

        // set fullscreen
        if (event->fullscreen != toplevel->fullscreen())
            toplevel->set_fullscreen(event->fullscreen);
        toplevel->update_foreign_toplevel();
    };
    wl_signal_add(&handle->events.request_fullscreen,
                  &handle_request_fullscreen);

    // handle_request_activate
    handle_request_activate.notify = [](wl_listener *listener,
                                        [[maybe_unused]] void *data) {
        Toplevel *toplevel =
            wl_container_of(listener, toplevel, handle_request_activate);

        // send focus
        toplevel->focus();
        toplevel->update_foreign_toplevel();
    };
    wl_signal_add(&handle->events.request_activate, &handle_request_activate);

    // handle_request_close
    handle_request_close.notify = [](wl_listener *listener,
                                     [[maybe_unused]] void *data) {
        Toplevel *toplevel =
            wl_container_of(listener, toplevel, handle_request_close);

        // send close
        toplevel->close();
        toplevel->update_foreign_toplevel();
    };
    wl_signal_add(&handle->events.request_close, &handle_request_close);

    // handle_set_rectangle
    handle_set_rectangle.notify = [](wl_listener *listener, void *data) {
        Toplevel *toplevel =
            wl_container_of(listener, toplevel, handle_request_close);

        const auto *event =
            static_cast<wlr_foreign_toplevel_handle_v1_set_rectangle_event *>(
                data);

        // NOTE: this might be entirely incorrect
        toplevel->set_position_size(event->x, event->y, event->width,
                                    event->height);

        toplevel->update_foreign_toplevel();
    };
    wl_signal_add(&handle->events.set_rectangle, &handle_set_rectangle);

    // handle_destroy
    handle_destroy.notify = [](wl_listener *listener,
                               [[maybe_unused]] void *data) {
        Toplevel *toplevel =
            wl_container_of(listener, toplevel, handle_destroy);

        delete toplevel;
    };
    wl_signal_add(&handle->events.destroy, &handle_destroy);
}

// Toplevel from xdg toplevel
Toplevel::Toplevel(Server *server, wlr_xdg_toplevel *xdg_toplevel)
    : server(server), xdg_toplevel(xdg_toplevel) {
    // add the toplevel to the scene tree
    scene_tree = wlr_scene_xdg_surface_create(server->layers.floating,
                                              xdg_toplevel->base);
    scene_tree->node.data = this;
    xdg_toplevel->base->data = scene_tree;

    // create foreign toplevel handle
    create_handle();

    // xdg_toplevel_map
    map.notify = map_notify;
    wl_signal_add(&xdg_toplevel->base->surface->events.map, &map);

    // xdg_toplevel_unmap
    unmap.notify = unmap_notify;
    wl_signal_add(&xdg_toplevel->base->surface->events.unmap, &unmap);

    // xdg_toplevel_commit
    commit.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        // on surface state change
        Toplevel *toplevel = wl_container_of(listener, toplevel, commit);

        if (toplevel->xdg_toplevel->base->initial_commit)
            // let client pick dimensions
            wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, 0, 0);
    };
    wl_signal_add(&xdg_toplevel->base->surface->events.commit, &commit);

    // new_xdg_popup
    new_xdg_popup.notify = [](wl_listener *listener, void *data) {
        Toplevel *toplevel = wl_container_of(listener, toplevel, new_xdg_popup);

        // popups do not need to be tracked
        new Popup(static_cast<wlr_xdg_popup *>(data), toplevel->scene_tree,
                  toplevel->server);
    };
    wl_signal_add(&xdg_toplevel->base->events.new_popup, &new_xdg_popup);

    // xdg_toplevel_destroy
    destroy.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        Toplevel *toplevel = wl_container_of(listener, toplevel, destroy);
        delete toplevel;
    };
    wl_signal_add(&xdg_toplevel->events.destroy, &destroy);

    // request_move
    request_move.notify = [](wl_listener *listener,
                             [[maybe_unused]] void *data) {
        Toplevel *toplevel = wl_container_of(listener, toplevel, request_move);

        // start interactivity
        toplevel->begin_interactive(CURSORMODE_MOVE, 0);
    };
    wl_signal_add(&xdg_toplevel->events.request_move, &request_move);

    // request_resize
    request_resize.notify = [](wl_listener *listener, void *data) {
        Toplevel *toplevel =
            wl_container_of(listener, toplevel, request_resize);
        const auto *event = static_cast<wlr_xdg_toplevel_resize_event *>(data);

        // start interactivity
        toplevel->begin_interactive(CURSORMODE_RESIZE, event->edges);
    };
    wl_signal_add(&xdg_toplevel->events.request_resize, &request_resize);

    // request_maximize
    request_maximize.notify = [](wl_listener *listener,
                                 [[maybe_unused]] void *data) {
        Toplevel *toplevel =
            wl_container_of(listener, toplevel, request_maximize);

        toplevel->set_maximized(toplevel->xdg_toplevel->requested.maximized);
    };
    wl_signal_add(&xdg_toplevel->events.request_maximize, &request_maximize);

    // request_fullscreen
    request_fullscreen.notify = [](wl_listener *listener,
                                   [[maybe_unused]] void *data) {
        Toplevel *toplevel =
            wl_container_of(listener, toplevel, request_fullscreen);

        bool fullscreen = toplevel->xdg_toplevel->requested.fullscreen;

        if (toplevel->fullscreen() != fullscreen)
            toplevel->set_fullscreen(fullscreen);
    };
    wl_signal_add(&xdg_toplevel->events.request_fullscreen,
                  &request_fullscreen);

    // request_minimize
    request_minimize.notify = [](wl_listener *listener,
                                 [[maybe_unused]] void *data) {
        Toplevel *toplevel =
            wl_container_of(listener, toplevel, request_minimize);

        // move toplevel to the bottom
        wlr_scene_node_lower_to_bottom(&toplevel->scene_tree->node);

        // focus the next toplevel in the workspace
        if (Workspace *workspace = toplevel->server->get_workspace(toplevel))
            workspace->focus_next();
    };
    wl_signal_add(&xdg_toplevel->events.request_minimize, &request_minimize);
}

Toplevel::~Toplevel() {
#ifdef XWAYLAND
    if (xwayland_surface) {
        wl_list_remove(&activate.link);
        wl_list_remove(&associate.link);
        wl_list_remove(&dissociate.link);
        wl_list_remove(&configure.link);
        wl_list_remove(&xwayland_resize.link);
        wl_list_remove(&xwayland_move.link);
        wl_list_remove(&xwayland_maximize.link);
        wl_list_remove(&xwayland_fullscreen.link);
        wl_list_remove(&xwayland_close.link);
    } else {
#endif
        wl_list_remove(&map.link);
        wl_list_remove(&unmap.link);
        wl_list_remove(&commit.link);
        wl_list_remove(&new_xdg_popup.link);
        wl_list_remove(&request_move.link);
        wl_list_remove(&request_resize.link);
        wl_list_remove(&request_maximize.link);
        wl_list_remove(&request_fullscreen.link);
        wl_list_remove(&request_minimize.link);
#ifdef XWAYLAND
    }
#endif

    wl_list_remove(&destroy.link);
    wl_list_remove(&handle_request_maximize.link);
    wl_list_remove(&handle_request_minimize.link);
    wl_list_remove(&handle_request_fullscreen.link);
    wl_list_remove(&handle_request_activate.link);
    wl_list_remove(&handle_request_close.link);
    wl_list_remove(&handle_set_rectangle.link);
    wl_list_remove(&handle_destroy.link);
}

#ifdef XWAYLAND
// Toplevel from xwayland surface
Toplevel::Toplevel(Server *server, wlr_xwayland_surface *xwayland_surface)
    : server(server), xwayland_surface(xwayland_surface) {
    // create foreign toplevel handle
    create_handle();

    // destroy
    destroy.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        Toplevel *toplevel = wl_container_of(listener, toplevel, destroy);
        delete toplevel;
    };
    wl_signal_add(&xwayland_surface->events.destroy, &destroy);

    // activate
    activate.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        Toplevel *toplevel = wl_container_of(listener, toplevel, activate);
        const wlr_xwayland_surface *xwayland_surface =
            toplevel->xwayland_surface;

        if (!xwayland_surface->surface || !xwayland_surface->surface->mapped)
            return;

        if (!xwayland_surface->override_redirect)
            wlr_xwayland_surface_activate(toplevel->xwayland_surface, true);
    };
    wl_signal_add(&xwayland_surface->events.request_activate, &activate);

    // associate
    associate.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        Toplevel *toplevel = wl_container_of(listener, toplevel, associate);

        // map
        toplevel->map.notify = map_notify;
        wl_signal_add(&toplevel->xwayland_surface->surface->events.map,
                      &toplevel->map);

        // unmap
        toplevel->unmap.notify = unmap_notify;
        wl_signal_add(&toplevel->xwayland_surface->surface->events.unmap,
                      &toplevel->unmap);
    };
    wl_signal_add(&xwayland_surface->events.associate, &associate);

    // dissociate
    dissociate.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        Toplevel *toplevel = wl_container_of(listener, toplevel, dissociate);

        // unmap
        wl_list_remove(&toplevel->map.link);
        wl_list_remove(&toplevel->unmap.link);
        wl_list_remove(&toplevel->xwayland_commit.link);
    };
    wl_signal_add(&xwayland_surface->events.dissociate, &dissociate);

    // configure
    configure.notify = [](wl_listener *listener, void *data) {
        Toplevel *toplevel = wl_container_of(listener, toplevel, configure);

        const auto *event =
            static_cast<wlr_xwayland_surface_configure_event *>(data);

        // unconfigured
        if (!toplevel->xwayland_surface->surface ||
            !toplevel->xwayland_surface->surface->mapped) {
            wlr_xwayland_surface_configure(toplevel->xwayland_surface, event->x,
                                           event->y, event->width,
                                           event->height);
            return;
        }

        // unmanaged
        if (toplevel->xwayland_surface->override_redirect) {
            // set scene node position
            wlr_scene_node_set_position(&toplevel->scene_surface->buffer->node,
                                        event->x, event->y);

            // set position and size
            toplevel->set_position_size(event->x, event->y, event->width,
                                        event->height);
            return;
        }

        // send arrange
        toplevel->server->output_manager->arrange();
    };
    wl_signal_add(&xwayland_surface->events.request_configure, &configure);

    // resize
    xwayland_resize.notify = [](wl_listener *listener, void *data) {
        Toplevel *toplevel =
            wl_container_of(listener, toplevel, xwayland_resize);
        const auto *event = static_cast<wlr_xwayland_resize_event *>(data);

        toplevel->begin_interactive(CURSORMODE_RESIZE, event->edges);
    };
    wl_signal_add(&xwayland_surface->events.request_resize, &xwayland_resize);

    // move
    xwayland_move.notify = [](wl_listener *listener,
                              [[maybe_unused]] void *data) {
        Toplevel *toplevel = wl_container_of(listener, toplevel, xwayland_move);

        toplevel->begin_interactive(CURSORMODE_MOVE, 0);
    };
    wl_signal_add(&xwayland_surface->events.request_move, &xwayland_move);

    // maximize
    xwayland_maximize.notify = [](wl_listener *listener,
                                  [[maybe_unused]] void *data) {
        Toplevel *toplevel =
            wl_container_of(listener, toplevel, xwayland_maximize);

        toplevel->toggle_maximized();
    };
    wl_signal_add(&xwayland_surface->events.request_maximize,
                  &xwayland_maximize);

    // fullscreen
    xwayland_fullscreen.notify = [](wl_listener *listener,
                                    [[maybe_unused]] void *data) {
        Toplevel *toplevel =
            wl_container_of(listener, toplevel, xwayland_fullscreen);

        toplevel->toggle_fullscreen();
    };
    wl_signal_add(&xwayland_surface->events.request_fullscreen,
                  &xwayland_fullscreen);

    // close
    xwayland_close.notify = [](wl_listener *listener,
                               [[maybe_unused]] void *data) {
        Toplevel *toplevel =
            wl_container_of(listener, toplevel, xwayland_close);

        toplevel->close();
    };
    wl_signal_add(&xwayland_surface->events.request_close, &xwayland_close);
}
#endif

// focus keyboard to surface
void Toplevel::focus() const {
    // locked
    if (server->locked)
        return;

    wlr_seat *seat = server->seat;
    wlr_surface *prev_surface = seat->keyboard_state.focused_surface;

    if ((xdg_toplevel && xdg_toplevel->base && xdg_toplevel->base->surface)
#ifdef XWAYLAND
        || (xwayland_surface && xwayland_surface->surface &&
            xwayland_surface->surface->mapped)
#endif
    ) {
#ifdef XWAYLAND
        // handle xdg toplevel or xwayland surface
        wlr_surface *surface = xdg_toplevel ? xdg_toplevel->base->surface
                                            : xwayland_surface->surface;
#else
        wlr_surface *surface = xdg_toplevel->base->surface;
#endif

        // cannot focus unmapped surface
        if (!surface->mapped)
            return;

        // already focused
        if (prev_surface == surface)
            return;

        // deactivate previous surface
        if (prev_surface) {
            // xdg toplevel
            wlr_xdg_toplevel *prev_toplevel =
                wlr_xdg_toplevel_try_from_wlr_surface(prev_surface);

#ifdef XWAYLAND
            // xwayland surface
            wlr_xwayland_surface *prev_xwayland_surface =
                wlr_xwayland_surface_try_from_wlr_surface(prev_surface);
#endif
            // check xdg toplevel
            if (prev_toplevel)
                wlr_xdg_toplevel_set_activated(prev_toplevel, false);
#ifdef XWAYLAND
            // check xwayland surface
            else if (prev_xwayland_surface)
                wlr_xwayland_surface_activate(prev_xwayland_surface, false);
#endif
        }

        // get keyboard
        wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);

        // move toplevel node to top of scene tree
        wlr_scene_node_raise_to_top(&scene_tree->node);

        // activate toplevel
        if (xdg_toplevel)
            wlr_xdg_toplevel_set_activated(xdg_toplevel, true);
#ifdef XWAYLAND
        else
            wlr_xwayland_surface_activate(xwayland_surface, true);
#endif

        // set seat keyboard focused surface to toplevel
        if (keyboard)
            wlr_seat_keyboard_notify_enter(seat, surface, keyboard->keycodes,
                                           keyboard->num_keycodes,
                                           &keyboard->modifiers);
    }

    // notify clients
    if (IPC *ipc = server->ipc)
        ipc->notify_clients(IPC_TOPLEVEL_LIST);
}

// move or resize toplevel
void Toplevel::begin_interactive(const CursorMode mode, const uint32_t edges) {
    server->grabbed_toplevel = this;

    Cursor *cursor = server->cursor;
    cursor->cursor_mode = mode;

    if (mode == CURSORMODE_MOVE) {
        if ((xdg_toplevel && xdg_toplevel->current.maximized)
#ifdef XWAYLAND
            || (xwayland_surface && xwayland_surface->maximized_horz &&
                xwayland_surface->maximized_vert)
#endif
        ) {
            // unmaximize if maximized
            saved_geometry.y = cursor->cursor->y - 10; // TODO Magic number here
            saved_geometry.x = cursor->cursor->x - (saved_geometry.width / 2.0);

            set_maximized(false);
        }

        // follow cursor
        cursor->grab_x = cursor->cursor->x - scene_tree->node.x;
        cursor->grab_y = cursor->cursor->y - scene_tree->node.y;
    } else {
        // don't resize fullscreened windows
        if (xdg_toplevel && xdg_toplevel->current.fullscreen)
            return;
#ifdef XWAYLAND
        else if (xwayland_surface && xwayland_surface->fullscreen)
            return;
#endif

        // get toplevel geometry
        const wlr_box geo_box = get_geometry();

        // calculate borders
        double border_x = (scene_tree->node.x + geo_box.x) +
                          ((edges & WLR_EDGE_RIGHT) ? geo_box.width : 0);
        double border_y = (scene_tree->node.y + geo_box.y) +
                          ((edges & WLR_EDGE_BOTTOM) ? geo_box.height : 0);

        // move with border
        cursor->grab_x = cursor->cursor->x - border_x;
        cursor->grab_y = cursor->cursor->y - border_y;

        // change size
        cursor->grab_geobox = geo_box;
        cursor->grab_geobox.x += scene_tree->node.x;
        cursor->grab_geobox.y += scene_tree->node.y;

        // set edges
        cursor->resize_edges = edges;
    }

    // notify clients
    if (IPC *ipc = server->ipc)
        ipc->notify_clients(IPC_TOPLEVEL_LIST);
}

// set the position and size of a toplevel, send a configure
void Toplevel::set_position_size(const double x, const double y, int width,
                                 int height) {
    // get output at cursor
    const wlr_output *wlr_output = server->focused_output()->wlr_output;

    // get output scale
    const float scale = wlr_output->scale;

    // enforce minimum size
    width = std::max(width, 1);
    height = std::max(height, 1);

    // toggle maximized if maximized
    if (maximized()) {
#ifdef XWAYLAND
        if (xdg_toplevel)
#endif
            wlr_xdg_toplevel_set_maximized(xdg_toplevel, false);
#ifdef XWAYLAND
        else
            wlr_xwayland_surface_set_maximized(xwayland_surface, false, false);
#endif
    } else
        // save current geometry
        save_geometry();

    // update geometry
#ifdef XWAYLAND
    if (xdg_toplevel) {
#endif
        // set new position
        wlr_scene_node_set_position(&scene_tree->node, x, y);

        // set position and size
        wlr_xdg_toplevel_set_size(xdg_toplevel, width / scale, height / scale);

        // schedule configure
        wlr_xdg_surface_schedule_configure(xdg_toplevel->base);
#ifdef XWAYLAND
    } else {
        // set scene node position
        wlr_scene_node_set_position(&scene_surface->buffer->node, x, y);

        // schedule configure
        wlr_xwayland_surface_configure(xwayland_surface, x, y, width, height);
    }
#endif

    geometry = wlr_box{static_cast<int>(x), static_cast<int>(y), width, height};

    // notify clients
    if (IPC *ipc = server->ipc) {
        ipc->notify_clients(IPC_TOPLEVEL_LIST);
        ipc->notify_clients(IPC_WORKSPACE_LIST);
    }
}

void Toplevel::set_position_size(const wlr_box &geometry) {
    set_position_size(geometry.x, geometry.y, geometry.width, geometry.height);
}

// get the geometry of the toplevel
wlr_box Toplevel::get_geometry() {
#ifdef XWAYLAND
    if (xdg_toplevel) {
#endif
        geometry.x = scene_tree->node.x;
        geometry.y = scene_tree->node.y;
        geometry.width = xdg_toplevel->base->surface->current.width;
        geometry.height = xdg_toplevel->base->surface->current.height;

        // this is called lying
        return xdg_toplevel->base->geometry;
#ifdef XWAYLAND
    } else {
        geometry.x = xwayland_surface->x;
        geometry.y = xwayland_surface->y;
        geometry.width = xwayland_surface->surface->current.width;
        geometry.height = xwayland_surface->surface->current.height;
        return geometry;
    }
#endif
}

// set the visibility of the toplevel
void Toplevel::set_hidden(const bool hidden) {
    this->hidden = hidden;

#ifdef XWAYLAND
    if (xdg_toplevel)
#endif
        wlr_scene_node_set_enabled(&scene_tree->node, !hidden);
#ifdef XWAYLAND
    else
        wlr_scene_node_set_enabled(&scene_surface->buffer->node, !hidden);
#endif

    if (IPC *ipc = server->ipc)
        ipc->notify_clients(IPC_TOPLEVEL_LIST);
}

// returns true if the toplevel is maximized
bool Toplevel::maximized() const {
#ifdef XWAYLAND
    if (xdg_toplevel)
#endif
        return xdg_toplevel->current.maximized;
#ifdef XWAYLAND
    else
        return xwayland_surface->maximized_horz ||
               xwayland_surface->maximized_vert;
#endif
}

// returns true if the toplevel is fullscreen
bool Toplevel::fullscreen() const {
#ifdef XWAYLAND
    if (xdg_toplevel)
#endif
        return xdg_toplevel->current.fullscreen ||
               handle->state & WLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN;
#ifdef XWAYLAND
    else
        return xwayland_surface->fullscreen ||
               handle->state & WLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN;
#endif
};

// set the toplevel to be fullscreened
void Toplevel::set_fullscreen(const bool fullscreen) {
    // get output
    const Output *output = server->focused_output();

    // get output geometry
    wlr_box output_box = output->layout_geometry;

    // set toplevel window mode to fullscreen
    if (xdg_toplevel)
        wlr_xdg_toplevel_set_fullscreen(xdg_toplevel, fullscreen);
#ifdef XWAYLAND
    else
        wlr_xwayland_surface_set_fullscreen(xwayland_surface, fullscreen);
#endif

    if (fullscreen) {
        // save current geometry
        save_geometry();

        // move scene tree node to fullscreen tree
        wlr_scene_node_raise_to_top(&scene_tree->node);
        wlr_scene_node_reparent(&scene_tree->node, server->layers.fullscreen);

        // set to top left of output, width and height the size of output
        set_position_size(output_box.x, output_box.y, output_box.width,
                          output_box.height);
    } else {
        // move scene tree node to toplevel tree
        wlr_scene_node_reparent(&scene_tree->node, server->layers.floating);

        // handles edge case where toplevel starts fullscreened
        if (saved_geometry.width && saved_geometry.height)
            // set back to saved geometry
            set_position_size(saved_geometry.x, saved_geometry.y,
                              saved_geometry.width, saved_geometry.height);
        else
            // use half of output geometry
            set_position_size(output_box.x, output_box.y, output_box.width / 2,
                              output_box.height / 2);
    }
}

// set the toplevel to be maximized
void Toplevel::set_maximized(const bool maximized) {
    if (xdg_toplevel) {
        // HACK: do not maximize if not initialized
        if (!xdg_toplevel->base->initialized)
            return;

        // unfullscreen if fullscreened
        if (xdg_toplevel->current.fullscreen)
            wlr_xdg_toplevel_set_fullscreen(xdg_toplevel, false);
    }
#ifdef XWAYLAND
    else if (xwayland_surface && xwayland_surface->fullscreen)
        wlr_xwayland_surface_set_fullscreen(xwayland_surface, false);
#endif

    // get output
    const Output *output = server->focused_output();

    // get usable output area
    wlr_box usable_area = output->usable_area;
    wlr_box output_box = output->layout_geometry;

    // set toplevel window mode to maximized
#ifdef XWAYLAND
    if (xdg_toplevel)
#endif
        wlr_xdg_toplevel_set_maximized(xdg_toplevel, maximized);
#ifdef XWAYLAND
    else
        wlr_xwayland_surface_set_maximized(xwayland_surface, maximized,
                                           maximized);
#endif

    if (maximized) {
        // save current geometry
        save_geometry();

        // set to top left of output, width and height the size of output
        set_position_size(usable_area.x + output_box.x,
                          usable_area.y + output_box.y, usable_area.width,
                          usable_area.height);
    } else
        // handles edge case where toplevel starts maximized
        if (saved_geometry.width && saved_geometry.height)
            // set back to saved geometry
            set_position_size(saved_geometry.x, saved_geometry.y,
                              saved_geometry.width, saved_geometry.height);
        else
            // use half of output geometry
            set_position_size(output_box.x, output_box.y, output_box.width / 2,
                              output_box.height / 2);
}

// update foreign toplevel on window state change
void Toplevel::update_foreign_toplevel() const {
    // update foreign state with xdg toplevel state
    wlr_foreign_toplevel_handle_v1_set_maximized(
        handle, xdg_toplevel->current.maximized);
    wlr_foreign_toplevel_handle_v1_set_fullscreen(
        handle, xdg_toplevel->current.fullscreen);
    wlr_foreign_toplevel_handle_v1_set_activated(
        handle, xdg_toplevel->current.activated);
}

// toggle maximized
void Toplevel::toggle_maximized() { set_maximized(!maximized()); }

// toggle fullscreen
void Toplevel::toggle_fullscreen() { set_fullscreen(!fullscreen()); }

// save the current geometry of the toplevel to saved_geometry
void Toplevel::save_geometry() {
    wlr_box geo = get_geometry();
    saved_geometry.x = geometry.x;
    saved_geometry.y = geometry.y;

    // current width and height are not set when a toplevel is created, but
    // saved geometry is
    if (geo.width && geo.height) {
#ifdef XWAYLAND
        if (xdg_toplevel) {
            saved_geometry.width = geo.width;
            saved_geometry.height = geo.height;
        } else {
            saved_geometry.width = xwayland_surface->surface->current.width;
            saved_geometry.height = xwayland_surface->surface->current.height;
        }
#else
        saved_geometry.width = geo.width;
        saved_geometry.height = geo.height;
#endif
    }
}

std::string Toplevel::title() const {
#ifdef XWAYLAND
    if (xdg_toplevel)
        return xdg_toplevel->title ? xdg_toplevel->title : "";
    else if (xwayland_surface)
        return xwayland_surface->title ? xwayland_surface->title : "";
#endif
    return xdg_toplevel->title ? xdg_toplevel->title : "";
}

// tell the toplevel to close
void Toplevel::close() const {
#ifdef XWAYLAND
    if (xdg_toplevel)
#endif
        wlr_xdg_toplevel_send_close(xdg_toplevel);
#ifdef XWAYLAND
    else if (xwayland_surface)
        wlr_xwayland_surface_close(xwayland_surface);
#endif
}
