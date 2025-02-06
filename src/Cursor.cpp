#include "Server.h"

Cursor::Cursor(struct Server *server) {
    // create wlr cursor and xcursor
    this->server = server;
    cursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(cursor, server->output_layout);
    cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
    cursor_mode = CURSORMODE_PASSTHROUGH;

    // cursor_motion
    motion.notify = [](struct wl_listener *listener, void *data) {
        // relative motion event
        struct Cursor *cursor = wl_container_of(listener, cursor, motion);
        struct wlr_pointer_motion_event *event =
            (wlr_pointer_motion_event *)data;

        wlr_cursor_move(cursor->cursor, &event->pointer->base, event->delta_x,
                        event->delta_y);
        cursor->process_motion(event->time_msec);
    };
    wl_signal_add(&cursor->events.motion, &motion);

    // cursor_motion_absolute
    motion_absolute.notify = [](struct wl_listener *listener, void *data) {
        // absolute motion event
        struct Cursor *cursor =
            wl_container_of(listener, cursor, motion_absolute);
        struct wlr_pointer_motion_absolute_event *event =
            (wlr_pointer_motion_absolute_event *)data;

        wlr_cursor_warp_absolute(cursor->cursor, &event->pointer->base,
                                 event->x, event->y);
        cursor->process_motion(event->time_msec);
    };
    wl_signal_add(&cursor->events.motion_absolute, &motion_absolute);

    // cursor_button
    button.notify = [](struct wl_listener *listener, void *data) {
        struct Cursor *cursor = wl_container_of(listener, cursor, button);
        struct wlr_pointer_button_event *event =
            (wlr_pointer_button_event *)data;

        // forward to seat
        wlr_seat_pointer_notify_button(cursor->server->seat, event->time_msec,
                                       event->button, event->state);

        if (event->state == WL_POINTER_BUTTON_STATE_RELEASED)
            cursor->reset_mode();
        else {
            Server *server = cursor->server;

            // focus surface
            double sx, sy;
            struct wlr_surface *surface = NULL;
            struct LayerSurface *layer_surface = server->layer_surface_at(
                cursor->cursor->x, cursor->cursor->y, &surface, &sx, &sy);
            if (layer_surface && surface)
                if (layer_surface->should_focus()) {
                    layer_surface->handle_focus();
                    return;
                }

            struct Toplevel *toplevel = server->toplevel_at(
                cursor->cursor->x, cursor->cursor->y, &surface, &sx, &sy);
            if (toplevel != NULL)
                toplevel->focus();
        }
    };
    wl_signal_add(&cursor->events.button, &button);

    // cursor_axis
    axis.notify = [](struct wl_listener *listener, void *data) {
        // scroll wheel etc
        struct Cursor *cursor = wl_container_of(listener, cursor, axis);

        struct wlr_pointer_axis_event *event = (wlr_pointer_axis_event *)data;

        // forward to seat
        wlr_seat_pointer_notify_axis(cursor->server->seat, event->time_msec,
                                     event->orientation, event->delta,
                                     event->delta_discrete, event->source,
                                     event->relative_direction);
    };
    wl_signal_add(&cursor->events.axis, &axis);

    // cursor_frame
    frame.notify = [](struct wl_listener *listener, void *data) {
        struct Cursor *cursor = wl_container_of(listener, cursor, frame);

        // forward to seat
        wlr_seat_pointer_notify_frame(cursor->server->seat);
    };
    wl_signal_add(&cursor->events.frame, &frame);
}

Cursor::~Cursor() {
    wl_list_remove(&motion.link);
    wl_list_remove(&motion_absolute.link);
    wl_list_remove(&button.link);
    wl_list_remove(&axis.link);
    wl_list_remove(&frame.link);

    wlr_cursor_destroy(cursor);
    wlr_xcursor_manager_destroy(cursor_mgr);
}

// deactivate cursor
void Cursor::reset_mode() {
    cursor_mode = CURSORMODE_PASSTHROUGH;
    server->grabbed_toplevel = NULL;
}

void Cursor::process_motion(uint32_t time) {
    // move or resize
    if (cursor_mode == CURSORMODE_MOVE) {
        process_move();
        return;
    } else if (cursor_mode == CURSORMODE_RESIZE) {
        process_resize();
        return;
    }

    // otherwise mode is passthrough
    double sx, sy;
    struct wlr_surface *surface = NULL;

    // get the toplevel under the cursor (if exists)
    struct Toplevel *toplevel =
        server->toplevel_at(cursor->x, cursor->y, &surface, &sx, &sy);
    if (toplevel)
        if (surface) {
            // connect the seat to the toplevel
            wlr_seat_pointer_notify_enter(server->seat, surface, sx, sy);
            wlr_seat_pointer_notify_motion(server->seat, time, sx, sy);
            return;
        }

    // get the layer surface under the cursor (if exists)
    struct LayerSurface *layer_surface =
        server->layer_surface_at(cursor->x, cursor->y, &surface, &sx, &sy);
    if (layer_surface)
        if (surface) {
            // connect the seat to the layer surface
            wlr_seat_pointer_notify_enter(server->seat, surface, sx, sy);
            wlr_seat_pointer_notify_motion(server->seat, time, sx, sy);
            return;
        }

    // set default cursor mode
    wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");
    wlr_seat_pointer_clear_focus(server->seat);
}

// move a toplevel
void Cursor::process_move() {
    if (server->grabbed_toplevel->xdg_toplevel->current.fullscreen)
        return;

    wlr_scene_node_set_position(&server->grabbed_toplevel->scene_tree->node,
                                cursor->x - grab_x, cursor->y - grab_y);
}

// resize a toplevel
void Cursor::process_resize() {
    // tinywl resize
    struct Toplevel *toplevel = server->grabbed_toplevel;
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
