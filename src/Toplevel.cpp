#include "Server.h"

void Toplevel::map_notify(struct wl_listener *listener, void *data) {
    // on map or display
    struct Toplevel *toplevel = wl_container_of(listener, toplevel, map);

    struct wlr_xdg_toplevel *xdg_toplevel = toplevel->xdg_toplevel;

    // xdg toplevel
    if (xdg_toplevel) {

        // get the focused output
        Output *output = toplevel->server->focused_output();

        if (output) {
            // set the fractional scale for this surface
            float scale = output->wlr_output->scale;
            wlr_fractional_scale_v1_notify_scale(xdg_toplevel->base->surface,
                                                 scale);
            wlr_surface_set_preferred_buffer_scale(xdg_toplevel->base->surface,
                                                   ceil(scale));

            // get usable area of the output
            struct wlr_box usable_area = output->usable_area;

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
            width = std::min(width, (uint32_t)usable_area.width);
            height = std::min(height, (uint32_t)usable_area.height);

            struct wlr_box output_box;
            wlr_output_layout_get_box(toplevel->server->output_manager->layout,
                                      output->wlr_output, &output_box);

            int32_t x = output_box.x + (usable_area.width - width) / 2;
            int32_t y = output_box.y + (usable_area.height - height) / 2;

            // ensure position falls in bounds
            x = std::max(x, usable_area.x);
            y = std::max(y, usable_area.y);

            // set the position
            wlr_scene_node_set_position(&toplevel->scene_tree->node, x, y);

            // add toplevel to active workspace and focus it
            output->get_active()->add_toplevel(toplevel, true);
        }
    }
#ifdef XWAYLAND
    else {
        // xwayland surface

        // create scene tree
        toplevel->scene_tree =
            wlr_scene_tree_create(toplevel->server->layers.floating);
        wlr_scene_node_set_enabled(
            &toplevel->scene_tree->node,
            toplevel->xwayland_surface->override_redirect);
        toplevel->scene_tree->node.data = toplevel;

        wlr_scene_node_set_position(&toplevel->scene_tree->node,
                                    toplevel->xwayland_surface->x,
                                    toplevel->xwayland_surface->y);
        wlr_xwayland_surface_configure(
            toplevel->xwayland_surface, toplevel->xwayland_surface->x,
            toplevel->xwayland_surface->y, toplevel->xwayland_surface->width,
            toplevel->xwayland_surface->height);

        // get the focused output
        Output *output = toplevel->server->focused_output();

        // add to active workspace
        if (output)
            output->get_active()->add_toplevel(toplevel, true);
    }
#endif
}

void Toplevel::unmap_notify(struct wl_listener *listener, void *data) {
    struct Toplevel *toplevel = wl_container_of(listener, toplevel, unmap);

    // deactivate
    if (toplevel == toplevel->server->grabbed_toplevel)
        toplevel->server->cursor->reset_mode();

    // remove from workspace
    if (Workspace *workspace = toplevel->server->get_workspace(toplevel))
        workspace->close(toplevel);

    // remove link
    wl_list_remove(&toplevel->link);
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
    handle_request_maximize.notify = [](struct wl_listener *listener,
                                        void *data) {
        struct Toplevel *toplevel =
            wl_container_of(listener, toplevel, handle_request_maximize);

        wlr_foreign_toplevel_handle_v1_maximized_event *event =
            static_cast<wlr_foreign_toplevel_handle_v1_maximized_event *>(data);

        toplevel->set_maximized(event->maximized);
        toplevel->update_foreign_toplevel();
    };
    wl_signal_add(&handle->events.request_maximize, &handle_request_maximize);

    // handle_request_minimize
    handle_request_minimize.notify = [](struct wl_listener *listener,
                                        void *data) {
        struct Toplevel *toplevel =
            wl_container_of(listener, toplevel, handle_request_minimize);

        // wlr_foreign_toplevel_handle_v1_minimized_event *event =
        //     static_cast<wlr_foreign_toplevel_handle_v1_minimized_event
        //     *>(data);
        //
        //  FIXME: since awm doesn't support minimization, just update the
        //  toplevel

        notify_send(
            "Minimizing foreign toplevels is not supported, expect issues");

        toplevel->update_foreign_toplevel();
    };
    wl_signal_add(&handle->events.request_minimize, &handle_request_minimize);

    // handle_request_fullscreen
    handle_request_fullscreen.notify = [](struct wl_listener *listener,
                                          void *data) {
        struct Toplevel *toplevel =
            wl_container_of(listener, toplevel, handle_request_fullscreen);

        wlr_foreign_toplevel_handle_v1_fullscreen_event *event =
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
        toplevel->set_fullscreen(event->fullscreen);
        toplevel->update_foreign_toplevel();
    };
    wl_signal_add(&handle->events.request_fullscreen,
                  &handle_request_fullscreen);

    // handle_request_activate
    handle_request_activate.notify = [](struct wl_listener *listener,
                                        void *data) {
        struct Toplevel *toplevel =
            wl_container_of(listener, toplevel, handle_request_activate);

        // send focus
        toplevel->focus();
        toplevel->update_foreign_toplevel();
    };
    wl_signal_add(&handle->events.request_activate, &handle_request_activate);

    // handle_request_close
    handle_request_close.notify = [](struct wl_listener *listener, void *data) {
        struct Toplevel *toplevel =
            wl_container_of(listener, toplevel, handle_request_close);

        // send close
        toplevel->close();
        toplevel->update_foreign_toplevel();
    };
    wl_signal_add(&handle->events.request_close, &handle_request_close);

    // handle_set_rectangle
    handle_set_rectangle.notify = [](struct wl_listener *listener, void *data) {
        struct Toplevel *toplevel =
            wl_container_of(listener, toplevel, handle_request_close);

        wlr_foreign_toplevel_handle_v1_set_rectangle_event *event =
            static_cast<wlr_foreign_toplevel_handle_v1_set_rectangle_event *>(
                data);

        // NOTE: this might be entirely incorrect
        toplevel->set_position_size(event->x, event->y, event->width,
                                    event->height);

        toplevel->update_foreign_toplevel();
    };
    wl_signal_add(&handle->events.set_rectangle, &handle_set_rectangle);

    // handle_destroy
    handle_destroy.notify = [](struct wl_listener *listener, void *data) {
        struct Toplevel *toplevel =
            wl_container_of(listener, toplevel, handle_destroy);

        delete toplevel;
    };
    wl_signal_add(&handle->events.destroy, &handle_destroy);
}

// Toplevel from xdg toplevel
Toplevel::Toplevel(struct Server *server,
                   struct wlr_xdg_toplevel *xdg_toplevel) {
    // add the toplevel to the scene tree
    this->server = server;
    this->xdg_toplevel = xdg_toplevel;
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
    commit.notify = [](struct wl_listener *listener, void *data) {
        // on surface state change
        struct Toplevel *toplevel = wl_container_of(listener, toplevel, commit);
        struct wlr_xdg_toplevel *xdg_toplevel = toplevel->xdg_toplevel;

        if (xdg_toplevel->base->initial_commit)
            // let client pick dimensions
            wlr_xdg_toplevel_set_size(xdg_toplevel, 0, 0);
    };
    wl_signal_add(&xdg_toplevel->base->surface->events.commit, &commit);

    // xdg_toplevel_destroy
    destroy.notify = [](struct wl_listener *listener, void *data) {
        struct Toplevel *toplevel =
            wl_container_of(listener, toplevel, destroy);

        wl_list_remove(&toplevel->map.link);
        wl_list_remove(&toplevel->unmap.link);
        wl_list_remove(&toplevel->commit.link);
        wl_list_remove(&toplevel->destroy.link);
        wl_list_remove(&toplevel->request_move.link);
        wl_list_remove(&toplevel->request_resize.link);
        wl_list_remove(&toplevel->request_maximize.link);
        wl_list_remove(&toplevel->request_fullscreen.link);

        wl_list_remove(&toplevel->handle_request_maximize.link);
        wl_list_remove(&toplevel->handle_request_minimize.link);
        wl_list_remove(&toplevel->handle_request_fullscreen.link);
        wl_list_remove(&toplevel->handle_request_activate.link);
        wl_list_remove(&toplevel->handle_request_close.link);
        wl_list_remove(&toplevel->handle_set_rectangle.link);
        wl_list_remove(&toplevel->handle_destroy.link);

        delete toplevel;
    };
    wl_signal_add(&xdg_toplevel->events.destroy, &destroy);

    // request_move
    request_move.notify = [](struct wl_listener *listener, void *data) {
        struct Toplevel *toplevel =
            wl_container_of(listener, toplevel, request_move);

        // start interactivity
        toplevel->begin_interactive(CURSORMODE_MOVE, 0);
    };
    wl_signal_add(&xdg_toplevel->events.request_move, &request_move);

    // request_resize
    request_resize.notify = [](struct wl_listener *listener, void *data) {
        /* This event is raised when a client would like to begin an interactive
         * resize, typically because the user clicked on their client-side
         * decorations. Note that a more sophisticated compositor should check
         * the provided serial against a list of button press serials sent to
         * this client, to prevent the client from requesting this whenever they
         * want. */
        struct Toplevel *toplevel =
            wl_container_of(listener, toplevel, request_resize);
        struct wlr_xdg_toplevel_resize_event *event =
            static_cast<wlr_xdg_toplevel_resize_event *>(data);

        toplevel->begin_interactive(CURSORMODE_RESIZE, event->edges);
    };
    wl_signal_add(&xdg_toplevel->events.request_resize, &request_resize);

    // request_maximize
    request_maximize.notify = [](struct wl_listener *listener, void *data) {
        struct Toplevel *toplevel =
            wl_container_of(listener, toplevel, request_maximize);

        toplevel->set_maximized(toplevel->xdg_toplevel->requested.maximized);
    };
    wl_signal_add(&xdg_toplevel->events.request_maximize, &request_maximize);

    // request_fullscreen
    request_fullscreen.notify = [](struct wl_listener *listener, void *data) {
        struct Toplevel *toplevel =
            wl_container_of(listener, toplevel, request_fullscreen);

        toplevel->set_fullscreen(toplevel->xdg_toplevel->requested.fullscreen);
    };
    wl_signal_add(&xdg_toplevel->events.request_fullscreen,
                  &request_fullscreen);
}

Toplevel::~Toplevel() {}

// Toplevel from xwayland surface
#ifdef XWAYLAND
Toplevel::Toplevel(struct Server *server,
                   struct wlr_xwayland_surface *xwayland_surface) {
    this->server = server;
    this->xwayland_surface = xwayland_surface;

    // create foreign toplevel handle
    create_handle();

    // destroy
    destroy.notify = [](struct wl_listener *listener, void *data) {
        struct Toplevel *toplevel =
            wl_container_of(listener, toplevel, destroy);

        wl_list_remove(&toplevel->destroy.link);
        wl_list_remove(&toplevel->activate.link);
        wl_list_remove(&toplevel->associate.link);
        wl_list_remove(&toplevel->dissociate.link);
        wl_list_remove(&toplevel->configure.link);

        wl_list_remove(&toplevel->handle_request_maximize.link);
        wl_list_remove(&toplevel->handle_request_minimize.link);
        wl_list_remove(&toplevel->handle_request_fullscreen.link);
        wl_list_remove(&toplevel->handle_request_activate.link);
        wl_list_remove(&toplevel->handle_request_close.link);
        wl_list_remove(&toplevel->handle_set_rectangle.link);
        wl_list_remove(&toplevel->handle_destroy.link);

        delete toplevel;
    };
    wl_signal_add(&xwayland_surface->events.destroy, &destroy);

    // activate
    activate.notify = [](struct wl_listener *listener, void *data) {
        struct Toplevel *toplevel =
            wl_container_of(listener, toplevel, activate);

        if (toplevel->xwayland_surface->override_redirect)
            wlr_xwayland_surface_activate(toplevel->xwayland_surface, true);
    };
    wl_signal_add(&xwayland_surface->events.request_activate, &activate);

    // associate
    associate.notify = [](struct wl_listener *listener, void *data) {
        struct Toplevel *toplevel =
            wl_container_of(listener, toplevel, associate);

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
    dissociate.notify = [](struct wl_listener *listener, void *data) {
        struct Toplevel *toplevel =
            wl_container_of(listener, toplevel, dissociate);

        // unmap
        wl_list_remove(&toplevel->map.link);
        wl_list_remove(&toplevel->unmap.link);
    };
    wl_signal_add(&xwayland_surface->events.dissociate, &dissociate);

    // configure
    configure.notify = [](struct wl_listener *listener, void *data) {
        struct Toplevel *toplevel =
            wl_container_of(listener, toplevel, configure);

        struct wlr_xwayland_surface_configure_event *event =
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
            wlr_scene_node_set_position(&toplevel->scene_tree->node, event->x,
                                        event->y);
            wlr_xwayland_surface_configure(toplevel->xwayland_surface, event->x,
                                           event->y, event->width,
                                           event->height);
            return;
        }

        // set position and size
        toplevel->set_position_size(event->x, event->y, event->width,
                                    event->height);

        // send arrange
        toplevel->server->output_manager->arrange();
    };
    wl_signal_add(&xwayland_surface->events.request_configure, &configure);
}
#endif

// focus keyboard to surface
void Toplevel::focus() {
    if (!xdg_toplevel || !xdg_toplevel->base || !xdg_toplevel->base->surface)
        return;

    struct wlr_seat *seat = server->seat;
    struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;
    struct wlr_surface *surface = xdg_toplevel->base->surface;

    // cannot focus unmapped surface
    if (!surface->mapped)
        return;

    // already focused
    if (prev_surface == surface)
        return;

    if (prev_surface) {
        // deactivate previous surface
        struct wlr_xdg_toplevel *prev_toplevel =
            wlr_xdg_toplevel_try_from_wlr_surface(prev_surface);
        if (prev_toplevel != NULL)
            wlr_xdg_toplevel_set_activated(prev_toplevel, false);
    }

    // get keyboard
    struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);

    // move toplevel node to top of scene tree
    wlr_scene_node_raise_to_top(&scene_tree->node);

    // activate toplevel
    wlr_xdg_toplevel_set_activated(xdg_toplevel, true);

    // set seat keyboard focused surface to toplevel
    if (keyboard != NULL)
        wlr_seat_keyboard_notify_enter(seat, surface, keyboard->keycodes,
                                       keyboard->num_keycodes,
                                       &keyboard->modifiers);
}

// move or resize toplevel
void Toplevel::begin_interactive(enum CursorMode mode, uint32_t edges) {
    server->grabbed_toplevel = this;

    Cursor *cursor = server->cursor;
    cursor->cursor_mode = mode;

    if (mode == CURSORMODE_MOVE) {
        if (xdg_toplevel->current.maximized) {
            // unmaximize if maximized
            wlr_xdg_toplevel_set_size(xdg_toplevel, saved_geometry.width,
                                      saved_geometry.height);
            wlr_xdg_toplevel_set_maximized(xdg_toplevel, false);
            wlr_xdg_surface_schedule_configure(xdg_toplevel->base);
        }

        // follow cursor
        cursor->grab_x = cursor->cursor->x - scene_tree->node.x;
        cursor->grab_y = cursor->cursor->y - scene_tree->node.y;

        // move toplevel to different workspace if it's moved into other output
        Workspace *current = server->get_workspace(this);
        Workspace *target = server->focused_output()->get_active();
        if (!target->contains(this) && current) {
            current->move_to(this, target);

            struct wlr_box output_box = target->output->layout_geometry;

            double new_x = cursor->cursor->x - cursor->grab_x;
            double new_y = cursor->cursor->y - cursor->grab_y;

            // Adjust for output offset
            new_x -= output_box.x;
            new_y -= output_box.y;

            wlr_scene_node_set_position(&scene_tree->node, new_x, new_y);
        }
    } else {
        // don't resize fullscreened windows
        if (xdg_toplevel->current.fullscreen)
            return;

        // get toplevel geometry
        struct wlr_box *geo_box = &xdg_toplevel->base->geometry;

        // calcualate borders
        double border_x = (scene_tree->node.x + geo_box->x) +
                          ((edges & WLR_EDGE_RIGHT) ? geo_box->width : 0);
        double border_y = (scene_tree->node.y + geo_box->y) +
                          ((edges & WLR_EDGE_BOTTOM) ? geo_box->height : 0);

        // move with border
        cursor->grab_x = cursor->cursor->x - border_x;
        cursor->grab_y = cursor->cursor->y - border_y;

        // change size
        cursor->grab_geobox = *geo_box;
        cursor->grab_geobox.x += scene_tree->node.x;
        cursor->grab_geobox.y += scene_tree->node.y;

        // set edges
        cursor->resize_edges = edges;
    }
}

// set the position and size of a toplevel, send a configure
void Toplevel::set_position_size(double x, double y, int width, int height) {
    // get output layout at cursor
    struct wlr_output *wlr_output = server->focused_output()->wlr_output;

    if (xdg_toplevel) {
        // get output scale
        float scale = wlr_output->scale;

        if (xdg_toplevel->current.maximized)
            // unmaximize if maximized
            wlr_xdg_toplevel_set_maximized(xdg_toplevel, false);
        else {
            // save current geometry
            saved_geometry.x = scene_tree->node.x;
            saved_geometry.y = scene_tree->node.y;
            saved_geometry.width = xdg_toplevel->current.width;
            saved_geometry.height = xdg_toplevel->current.height;
        }

        // set position and size
        wlr_scene_node_set_position(&scene_tree->node, x, y);
        wlr_xdg_toplevel_set_size(xdg_toplevel, width / scale, height / scale);

        // schedule configure
        wlr_xdg_surface_schedule_configure(xdg_toplevel->base);
    }
#ifdef XWAYLAND
    else
        wlr_xwayland_surface_configure(xwayland_surface, x, y, width, height);
#endif
}

void Toplevel::set_position_size(struct wlr_fbox geometry) {
    set_position_size(geometry.x, geometry.y, geometry.width, geometry.height);
}

// get the geometry of the toplevel
struct wlr_fbox Toplevel::get_geometry() {
    struct wlr_fbox geometry;
    geometry.x = scene_tree->node.x;
    geometry.y = scene_tree->node.y;
    geometry.width = xdg_toplevel->current.width;
    geometry.height = xdg_toplevel->current.height;
    return geometry;
}

// set the visibility of the toplevel
void Toplevel::set_hidden(bool hidden) {
    this->hidden = hidden;
    wlr_scene_node_set_enabled(&scene_tree->node, !hidden);
}

// set the toplevel to be fullscreened
void Toplevel::set_fullscreen(bool fullscreen) {
    // cannot fullscreen
    if (!xdg_toplevel->base->initialized)
        return;

    // get output
    Output *output = server->focused_output();

    // get output scale
    float scale = output->wlr_output->scale;

    // get output geometry
    struct wlr_box output_box;
    wlr_output_layout_get_box(server->output_manager->layout,
                              output->wlr_output, &output_box);

    if (fullscreen) {
        // save current geometry
        saved_geometry.x = scene_tree->node.x;
        saved_geometry.y = scene_tree->node.y;
        saved_geometry.width = xdg_toplevel->current.width;
        saved_geometry.height = xdg_toplevel->current.height;

        // set to top left of output, width and height the size of output
        wlr_scene_node_set_position(&scene_tree->node, output_box.x,
                                    output_box.y);
        wlr_xdg_toplevel_set_size(xdg_toplevel, output_box.width / scale,
                                  output_box.height / scale);

        // move scene tree node to fullscreen tree
        wlr_scene_node_raise_to_top(&scene_tree->node);
        wlr_scene_node_reparent(&scene_tree->node, server->layers.fullscreen);
    } else {
        // set back to saved geometry
        wlr_scene_node_set_position(&scene_tree->node, saved_geometry.x,
                                    saved_geometry.y);
        wlr_xdg_toplevel_set_size(xdg_toplevel, saved_geometry.width / scale,
                                  saved_geometry.height / scale);

        // move scene tree node to toplevel tree
        wlr_scene_node_reparent(&scene_tree->node, server->layers.floating);
    }

    // set toplevel window mode to fullscreen
    wlr_xdg_toplevel_set_fullscreen(xdg_toplevel, fullscreen);

    // schedule configure
    wlr_xdg_surface_schedule_configure(xdg_toplevel->base);
}

// set the toplevel to be maximized
void Toplevel::set_maximized(bool maximized) {
    // cannot maximize
    if (!xdg_toplevel->base->initialized)
        return;

    // unfullscreen if fullscreened
    if (xdg_toplevel->current.fullscreen)
        wlr_xdg_toplevel_set_fullscreen(xdg_toplevel, false);

    // get output
    Output *output = server->focused_output();

    // get output scale
    float scale = output->wlr_output->scale;

    // get usable output area
    struct wlr_box output_box = output->usable_area;

    if (maximized) {
        // save current geometry
        saved_geometry.x = scene_tree->node.x;
        saved_geometry.y = scene_tree->node.y;
        saved_geometry.width = xdg_toplevel->current.width;
        saved_geometry.height = xdg_toplevel->current.height;

        // set to top left of output, width and height the size of output
        wlr_scene_node_set_position(&scene_tree->node,
                                    output_box.x + output->layout_geometry.x,
                                    output_box.y + output->layout_geometry.y);
        wlr_xdg_toplevel_set_size(xdg_toplevel, output_box.width / scale,
                                  output_box.height / scale);
    } else {
        // set back to saved geometry
        wlr_scene_node_set_position(&scene_tree->node, saved_geometry.x,
                                    saved_geometry.y);
        wlr_xdg_toplevel_set_size(xdg_toplevel, saved_geometry.width / scale,
                                  saved_geometry.height / scale);
    }
    // set toplevel window mode to maximized
    wlr_xdg_toplevel_set_maximized(xdg_toplevel, maximized);

    // schedule configure
    wlr_xdg_surface_schedule_configure(xdg_toplevel->base);
}

// update foreign toplevel on window state change
void Toplevel::update_foreign_toplevel() {
    // update foreign state with xdg toplevel state
    wlr_foreign_toplevel_handle_v1_set_maximized(
        handle, xdg_toplevel->current.maximized);
    wlr_foreign_toplevel_handle_v1_set_fullscreen(
        handle, xdg_toplevel->current.fullscreen);
    wlr_foreign_toplevel_handle_v1_set_activated(
        handle, xdg_toplevel->current.activated);
}

// tell the toplevel to close
void Toplevel::close() {
    if (xdg_toplevel == nullptr)
        return;

    wlr_xdg_toplevel_send_close(xdg_toplevel);
}
