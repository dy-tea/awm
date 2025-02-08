#include "Server.h"

Toplevel::Toplevel(struct Server *server,
                   struct wlr_xdg_toplevel *xdg_toplevel) {
    // add the toplevel to the scene tree
    this->server = server;
    this->xdg_toplevel = xdg_toplevel;
    scene_tree =
        wlr_scene_xdg_surface_create(&server->scene->tree, xdg_toplevel->base);
    scene_tree->node.data = this;
    xdg_toplevel->base->data = scene_tree;

    // set foreign toplevel initial state if received
    handle = wlr_foreign_toplevel_handle_v1_create(
        server->wlr_foreign_toplevel_manager);

    if (xdg_toplevel->title)
        wlr_foreign_toplevel_handle_v1_set_title(handle, xdg_toplevel->title);

    if (xdg_toplevel->app_id)
        wlr_foreign_toplevel_handle_v1_set_app_id(handle, xdg_toplevel->app_id);

    // xdg_toplevel_map
    map.notify = [](struct wl_listener *listener, void *data) {
        /* Called when the surface is mapped, or ready to display on-screen. */
        struct Toplevel *toplevel = wl_container_of(listener, toplevel, map);

        // get the focused output
        Output *active = toplevel->server->focused_output();

        // add toplevel to active workspace and focus it
        if (active) {
            active->get_active()->add_toplevel(toplevel);
            toplevel->focus();
        }
    };
    wl_signal_add(&xdg_toplevel->base->surface->events.map, &map);

    // xdg_toplevel_unmap
    unmap.notify = [](struct wl_listener *listener, void *data) {
        struct Toplevel *toplevel = wl_container_of(listener, toplevel, unmap);

        // deactivate
        if (toplevel == toplevel->server->grabbed_toplevel)
            toplevel->server->cursor->reset_mode();

        wl_list_remove(&toplevel->link);
    };
    wl_signal_add(&xdg_toplevel->base->surface->events.unmap, &unmap);

    // xdg_toplevel_commit
    commit.notify = [](struct wl_listener *listener, void *data) {
        /* Called when a new surface state is committed. */
        struct Toplevel *toplevel = wl_container_of(listener, toplevel, commit);

        if (toplevel->xdg_toplevel->base->initial_commit)
            // let client pick dimensions
            wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, 0, 0);
    };
    wl_signal_add(&xdg_toplevel->base->surface->events.commit, &commit);

    // xdg_toplevel_destroy
    destroy.notify = [](struct wl_listener *listener, void *data) {
        struct Toplevel *toplevel =
            wl_container_of(listener, toplevel, destroy);

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
            (wlr_xdg_toplevel_resize_event *)data;

        toplevel->update_foreign_toplevel();
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

    /*
     * foreign toplevel listeners
     */

    // handle_request_maximize
    handle_request_maximize.notify = [](struct wl_listener *listener,
                                        void *data) {
        struct Toplevel *toplevel =
            wl_container_of(listener, toplevel, handle_request_maximize);

        toplevel->update_foreign_toplevel();
        toplevel->set_maximized(toplevel->xdg_toplevel->requested.maximized);
    };
    wl_signal_add(&handle->events.request_maximize, &handle_request_maximize);

    // handle_request_fullscreen
    handle_request_fullscreen.notify = [](struct wl_listener *listener,
                                          void *data) {
        struct Toplevel *toplevel =
            wl_container_of(listener, toplevel, handle_request_fullscreen);

        toplevel->update_foreign_toplevel();
        toplevel->set_fullscreen(toplevel->xdg_toplevel->requested.fullscreen);
    };
    wl_signal_add(&handle->events.request_fullscreen,
                  &handle_request_fullscreen);
}

Toplevel::~Toplevel() {
    wl_list_remove(&map.link);
    wl_list_remove(&unmap.link);
    wl_list_remove(&commit.link);
    wl_list_remove(&destroy.link);
    wl_list_remove(&request_move.link);
    wl_list_remove(&request_resize.link);
    wl_list_remove(&request_maximize.link);
    wl_list_remove(&request_fullscreen.link);

    wl_list_remove(&handle_request_maximize.link);
    wl_list_remove(&handle_request_fullscreen.link);
}

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
    wlr_cursor *cursor = server->cursor->cursor;
    struct wlr_output *wlr_output = wlr_output_layout_output_at(
        server->output_layout, cursor->x, cursor->y);

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

    // get output layout
    wlr_cursor *cursor = server->cursor->cursor;
    struct wlr_output *wlr_output = wlr_output_layout_output_at(
        server->output_layout, cursor->x, cursor->y);

    // get output scale
    float scale = wlr_output->scale;

    // get output geometry
    struct wlr_box output_box;
    wlr_output_layout_get_box(server->output_layout, wlr_output, &output_box);

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
    } else {
        // set back to saved geometry
        wlr_scene_node_set_position(&scene_tree->node, output_box.x,
                                    output_box.y);
        wlr_xdg_toplevel_set_size(xdg_toplevel, saved_geometry.width / scale,
                                  saved_geometry.height / scale);
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

    // get output layout
    wlr_cursor *cursor = server->cursor->cursor;
    struct wlr_output *wlr_output = wlr_output_layout_output_at(
        server->output_layout, cursor->x, cursor->y);

    // get output scale
    float scale = wlr_output->scale;

    // get output geometry
    struct wlr_box output_box;
    wlr_output_layout_get_box(server->output_layout, wlr_output, &output_box);

    if (maximized) {
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
        handle, xdg_toplevel->current.maximized);
    wlr_foreign_toplevel_handle_v1_set_activated(
        handle, xdg_toplevel->current.maximized);
}

// tell the toplevel to close
void Toplevel::close() {
    if (xdg_toplevel == nullptr)
        return;

    wlr_xdg_toplevel_send_close(xdg_toplevel);
}
