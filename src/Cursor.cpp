#include "Server.h"
#include <pixman.h>

Cursor::Cursor(Server *server) : server(server) {
    // create wlr cursor and xcursor
    cursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(cursor, server->output_manager->layout);

    // size
    const int64_t size = server->config->cursor.xcursor.size;
    const char *theme = server->config->cursor.xcursor.theme.empty()
                            ? nullptr
                            : server->config->cursor.xcursor.theme.data();
    cursor_mgr = wlr_xcursor_manager_create(theme, size > 0 ? size : 24);

    cursor_mode = CURSORMODE_PASSTHROUGH;

    // cursor shape manager
    cursor_shape_mgr = wlr_cursor_shape_manager_v1_create(server->display, 1);

    // set cursor shape
    request_set_shape.notify = [](wl_listener *listener, void *data) {
        Cursor *cursor = wl_container_of(listener, cursor, request_set_shape);
        const auto *event =
            static_cast<wlr_cursor_shape_manager_v1_request_set_shape_event *>(
                data);

        if (cursor->cursor_mode != CURSORMODE_PASSTHROUGH)
            return;

        if (event->seat_client ==
            cursor->server->seat->pointer_state.focused_client)
            wlr_cursor_set_xcursor(cursor->cursor, cursor->cursor_mgr,
                                   wlr_cursor_shape_v1_name(event->shape));
    };
    wl_signal_add(&cursor_shape_mgr->events.request_set_shape,
                  &request_set_shape);

    // motion
    motion.notify = [](wl_listener *listener, void *data) {
        // relative motion event
        Cursor *cursor = wl_container_of(listener, cursor, motion);
        const auto *event = static_cast<wlr_pointer_motion_event *>(data);

        // process motion
        cursor->process_motion(event->time_msec, &event->pointer->base,
                               event->delta_x, event->delta_y,
                               event->unaccel_dx, event->unaccel_dy);
    };
    wl_signal_add(&cursor->events.motion, &motion);

    // motion_absolute
    motion_absolute.notify = [](wl_listener *listener, void *data) {
        // absolute motion event
        Cursor *cursor = wl_container_of(listener, cursor, motion_absolute);
        const auto *event =
            static_cast<wlr_pointer_motion_absolute_event *>(data);

        // warp cursor
        if (event->time_msec)
            wlr_cursor_warp_absolute(cursor->cursor, &event->pointer->base,
                                     event->x, event->y);

        // get absolute pos
        double lx, ly, dx, dy;
        wlr_cursor_absolute_to_layout_coords(cursor->cursor,
                                             &event->pointer->base, event->x,
                                             event->y, &lx, &ly);
        dx = lx - cursor->cursor->x;
        dy = ly - cursor->cursor->y;

        // process motion
        cursor->process_motion(event->time_msec, &event->pointer->base, dx, dy,
                               dx, dy);
    };
    wl_signal_add(&cursor->events.motion_absolute, &motion_absolute);

    // button
    button.notify = [](wl_listener *listener, void *data) {
        Cursor *cursor = wl_container_of(listener, cursor, button);
        Server *server = cursor->server;
        const auto *event = static_cast<wlr_pointer_button_event *>(data);

        // notify activity
        wlr_idle_notifier_v1_notify_activity(server->wlr_idle_notifier,
                                             server->seat);

        // forward to seat
        wlr_seat_pointer_notify_button(server->seat, event->time_msec,
                                       event->button, event->state);

        if (event->state == WL_POINTER_BUTTON_STATE_RELEASED)
            // show standard pointer cursor
            cursor->reset_mode();
        else {
            // handle cursor focus
            Server *server = cursor->server;

            double sx, sy;
            wlr_surface *surface = nullptr;

            // layer surface focus
            LayerSurface *layer_surface = server->layer_surface_at(
                cursor->cursor->x, cursor->cursor->y, &surface, &sx, &sy);
            if (layer_surface && surface && surface->mapped)
                layer_surface->handle_focus();

            // toplevel focus
            Toplevel *toplevel = server->toplevel_at(
                cursor->cursor->x, cursor->cursor->y, &surface, &sx, &sy);
            if (toplevel && surface && surface->mapped)
                server->focused_output()->get_active()->focus_toplevel(
                    toplevel);

            // add to pressed buttons
            cursor->pressed_buttons |= 1 << (event->button - 272);
        }

        // emit a keyboard event for the pressed button
        Keyboard *keyboard =
            wl_container_of(server->keyboards.next, keyboard, link);
        uint32_t time_msec = event->time_msec;
        wlr_keyboard_key_event *key_event = new wlr_keyboard_key_event{
            time_msec,
            0,
            false,
            static_cast<wl_keyboard_key_state>(event->state),
        };
        wl_signal_emit(&keyboard->wlr_keyboard->events.key, key_event);
        delete key_event;
    };
    wl_signal_add(&cursor->events.button, &button);

    // axis
    axis.notify = [](wl_listener *listener, void *data) {
        // scroll wheel etc
        Cursor *cursor = wl_container_of(listener, cursor, axis);
        Server *server = cursor->server;

        const auto *event = static_cast<wlr_pointer_axis_event *>(data);

        // notify activity
        wlr_idle_notifier_v1_notify_activity(server->wlr_idle_notifier,
                                             server->seat);

        // forward to seat
        wlr_seat_pointer_notify_axis(
            server->seat, event->time_msec, event->orientation, event->delta,
            event->delta_discrete, event->source, event->relative_direction);
    };
    wl_signal_add(&cursor->events.axis, &axis);

    // frame
    frame.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        Cursor *cursor = wl_container_of(listener, cursor, frame);

        // forward to seat
        wlr_seat_pointer_notify_frame(cursor->server->seat);
    };
    wl_signal_add(&cursor->events.frame, &frame);

    // constraint
    wl_list_init(&constraint_commit.link);

    // center cursor
    wlr_cursor_warp_closest(cursor, nullptr, cursor->x, cursor->y);
}

Cursor::~Cursor() {
    wl_list_remove(&motion.link);
    wl_list_remove(&motion_absolute.link);
    wl_list_remove(&button.link);
    wl_list_remove(&axis.link);
    wl_list_remove(&frame.link);
    wl_list_remove(&request_set_shape.link);

    wlr_cursor_destroy(cursor);
    wlr_xcursor_manager_destroy(cursor_mgr);
}

// deactivate cursor
void Cursor::reset_mode() {
    cursor_mode = CURSORMODE_PASSTHROUGH;
    pressed_buttons = 0;
    server->grabbed_toplevel = nullptr;
}

void Cursor::process_motion(uint32_t time, wlr_input_device *device, double dx,
                            double dy, double unaccel_dx, double unaccel_dy) {
    if (time) {
        // send relative motion event
        wlr_relative_pointer_manager_v1_send_relative_motion(
            server->wlr_relative_pointer_manager, server->seat,
            static_cast<uint64_t>(time) * 1000, dx, dy, unaccel_dx, unaccel_dy);

        // constrain cursor
        wlr_pointer_constraint_v1 *constraint;
        wl_list_for_each(constraint,
                         &server->wlr_pointer_constraints->constraints, link)
            constrain(constraint);

        if (active_constraint && cursor_mode != CURSORMODE_RESIZE &&
            cursor_mode != CURSORMODE_MOVE) {
            // get the toplevel which the constraint is applied to
            const auto *toplevel =
                server->get_toplevel(active_constraint->surface);

            // ensure toplevel is focused
            if (toplevel && active_constraint->surface ==
                                server->seat->pointer_state.focused_surface) {
                const wlr_box geo_box = toplevel->geometry;

                // calculate constraint
                double sx = cursor->x - geo_box.x - geo_box.width;
                double sy = cursor->y - geo_box.y - geo_box.height;
                double cx, cy;

                // apply confine on region
                if (wlr_region_confine(&active_constraint->region, sx, sy,
                                       sx + dx, sy + dy, &cx, &cy)) {
                    dx = cx - sx;
                    dy = cy - sy;
                }

                // if pointer is locked, do not move it
                if (active_constraint->type == WLR_POINTER_CONSTRAINT_V1_LOCKED)
                    return;
            }
        }
    }

    // update drag icon position
    wlr_scene_node_set_position(&server->layers.drag_icon->node, cursor->x,
                                cursor->y);

    // move cursor
    wlr_cursor_move(cursor, device, dx, dy);

    // notify activity
    wlr_idle_notifier_v1_notify_activity(server->wlr_idle_notifier,
                                         server->seat);

    // move or resize toplevel
    if (cursor_mode == CURSORMODE_MOVE) {
        process_move();
        return;
    } else if (cursor_mode == CURSORMODE_RESIZE) {
        process_resize();
        return;
    }

    // otherwise mode is passthrough
    double sx, sy;
    wlr_surface *surface = nullptr;

    // get the toplevel under the cursor (if exists)
    const Toplevel *toplevel =
        server->toplevel_at(cursor->x, cursor->y, &surface, &sx, &sy);
    if (toplevel && surface && surface->mapped) {
        // connect the seat to the toplevel
        wlr_seat_pointer_notify_enter(server->seat, surface, sx, sy);
        wlr_seat_pointer_notify_motion(server->seat, time, sx, sy);

        // focus on hover
        if (server->config->general.focus_on_hover)
            toplevel->focus();
        return;
    }

    // get the layer surface under the cursor (if exists)
    LayerSurface *layer_surface =
        server->layer_surface_at(cursor->x, cursor->y, &surface, &sx, &sy);
    if (layer_surface && surface && surface->mapped) {
        // connect the seat to the layer surface
        wlr_seat_pointer_notify_enter(server->seat, surface, sx, sy);
        wlr_seat_pointer_notify_motion(server->seat, time, sx, sy);

        // focus on hover
        if (server->config->general.focus_on_hover)
            layer_surface->handle_focus();
        return;
    }

    // set default cursor mode
    wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");
    wlr_seat_pointer_clear_focus(server->seat);
}

// move a toplevel
void Cursor::process_move() {
    // grabbed toplevel
    Toplevel *toplevel = server->grabbed_toplevel;

    // do not move fullscreen toplevel
    if (toplevel->fullscreen())
        return;

    // get the output of the current workspace
    Workspace *current = server->get_workspace(toplevel);

    // calculate new x and y based on cursor position
    double new_x = cursor->x - grab_x;
    double new_y = cursor->y - grab_y;

    // set the new position
    wlr_scene_node_set_position(&toplevel->scene_tree->node, new_x, new_y);

    // update position
    toplevel->geometry.x = new_x;
    toplevel->geometry.y = new_y;

    // move toplevel to different workspace if it's moved into other output
    Output *output = server->focused_output();
    Workspace *target = output->get_active();
    if (!target->contains(toplevel) && current) {
        // set the new workspace
        current->move_to(toplevel, target);

        // handle foreign toplevel
        if (toplevel->foreign_handle) {
            // notify of entered output
            wlr_foreign_toplevel_handle_v1_output_enter(
                toplevel->foreign_handle, output->wlr_output);

            // notify of left output
            if (wlr_box_contains_box(&output->layout_geometry,
                                     &toplevel->geometry))
                wlr_foreign_toplevel_handle_v1_output_leave(
                    toplevel->foreign_handle, current->output->wlr_output);
        }
    }

    // notify clients
    if (IPC *ipc = server->ipc)
        ipc->notify_clients(IPC_TOPLEVEL_LIST);
}

// resize a toplevel
void Cursor::process_resize() {
    Toplevel *toplevel = server->grabbed_toplevel;

    // do not resize fullscreen toplevel
    if (toplevel->fullscreen())
        return;

    // amalgamation of tinywl and dwl code
    double border_x = cursor->x - grab_x;
    double border_y = cursor->y - grab_y;
    int new_left = grab_geobox.x;
    int new_right = grab_geobox.x + grab_geobox.width;
    int new_top = grab_geobox.y;
    int new_bottom = grab_geobox.y + grab_geobox.height;

    // resize top and bottom edges
    if (resize_edges & WLR_EDGE_TOP) {
        new_top = border_y;
        if (new_top >= new_bottom)
            new_top = new_bottom - 1;
    } else if (resize_edges & WLR_EDGE_BOTTOM) {
        new_bottom = border_y;
        if (new_bottom <= new_top)
            new_bottom = new_top + 1;
    }

    // resize left and right edges
    if (resize_edges & WLR_EDGE_LEFT) {
        new_left = border_x;
        if (new_left >= new_right)
            new_left = new_right - 1;
    } else if (resize_edges & WLR_EDGE_RIGHT) {
        new_right = border_x;
        if (new_right <= new_left)
            new_right = new_left + 1;
    }

    wlr_box geo_box = toplevel->get_geometry();
    int new_x = new_left - geo_box.x;
    int new_y = new_top - geo_box.y;
    int new_width = new_right - new_left;
    int new_height = new_bottom - new_top;

    // wtf???
    if (resize_edges & WLR_EDGE_LEFT)
        new_x -= geo_box.width - new_width;
    if (resize_edges & WLR_EDGE_TOP)
        new_y -= geo_box.height - new_height;

    // set new geometry
    wlr_scene_node_set_position(&toplevel->scene_tree->node, new_x, new_y);

#ifdef XWAYLAND
    if (toplevel->xdg_toplevel) {
#endif
        wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, new_width,
                                  new_height);
        wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);
#ifdef XWAYLAND
    } else
        wlr_xwayland_surface_configure(toplevel->xwayland_surface, new_x, new_y,
                                       new_width, new_height);

    if (toplevel->xdg_toplevel) {
#endif
        // enforce minimum size
        new_width =
            std::max(new_width, toplevel->xdg_toplevel->current.min_width);
        new_height =
            std::max(new_height, toplevel->xdg_toplevel->current.min_height);
#ifdef XWAYLAND
    }
#endif

    toplevel->geometry = {new_x, new_y, new_width, new_height};

    // notify clients
    if (IPC *ipc = server->ipc)
        ipc->notify_clients(IPC_TOPLEVEL_LIST);
}

// constrain the cursor to a given pointer constraint
void Cursor::constrain(wlr_pointer_constraint_v1 *constraint) {
    // no change in constraint
    if (active_constraint == constraint)
        return;

    wl_list_remove(&constraint_commit.link);
    if (active_constraint) {
        if (!constraint)
            warp_to_constraint_hint();

        // deactivate current constraint
        wlr_pointer_constraint_v1_send_deactivated(active_constraint);
    }

    // set the new constraint
    active_constraint = constraint;

    if (!constraint) {
        wl_list_init(&constraint->link);
        return;
    }

    requires_warp = true;

    // sussy sway code
    if (pixman_region32_not_empty(&constraint->current.region))
        pixman_region32_intersect(&constraint->region,
                                  &constraint->surface->input_region,
                                  &constraint->current.region);
    else
        pixman_region32_copy(&constraint->region,
                             &constraint->surface->input_region);

    check_constraint_region();

    wlr_pointer_constraint_v1_send_activated(constraint);

    constraint_commit.notify = [](wl_listener *listener,
                                  [[maybe_unused]] void *data) {
        Cursor *cursor = wl_container_of(listener, cursor, constraint_commit);
        cursor->check_constraint_region();
    };
    wl_signal_add(&constraint->surface->events.commit, &constraint_commit);
}

// le sway code
void Cursor::check_constraint_region() {
    wlr_pointer_constraint_v1 *constraint = active_constraint;
    pixman_region32_t *region = &constraint->region;
    Toplevel *toplevel = server->get_toplevel(constraint->surface);
    if (requires_warp && toplevel) {
        requires_warp = false;

        double sx = cursor->x + toplevel->geometry.x;
        double sy = cursor->y + toplevel->geometry.y;

        if (!pixman_region32_contains_point(region, floor(sx), floor(sy),
                                            NULL)) {
            int count;
            pixman_box32_t *boxes = pixman_region32_rectangles(region, &count);
            if (count > 0) {
                sx = (boxes[0].x1 + boxes[0].x2) / 2.0;
                sy = (boxes[0].y1 + boxes[0].y2) / 2.0;

                wlr_cursor_warp_closest(cursor, NULL, sx + toplevel->geometry.x,
                                        sy + toplevel->geometry.y);
            }
        }
    }

    // empty region if locked
    if (constraint->type == WLR_POINTER_CONSTRAINT_V1_CONFINED)
        pixman_region32_copy(&confine, region);
    else
        pixman_region32_clear(&confine);
}

void Cursor::warp_to_constraint_hint() {
    if (active_constraint->current.cursor_hint.enabled) {
        double sx = active_constraint->current.cursor_hint.x;
        double sy = active_constraint->current.cursor_hint.y;

        Toplevel *toplevel = server->get_toplevel(active_constraint->surface);
        if (!toplevel)
            return;

        double lx = sx - toplevel->geometry.x;
        double ly = sy - toplevel->geometry.y;

        wlr_cursor_warp(cursor, NULL, lx, ly);
        wlr_seat_pointer_warp(active_constraint->seat, sx, sy);
    }
}

// returns true if the cursor is a touchpad
bool Cursor::is_touchpad(wlr_pointer *pointer) const {
    if (pointer->base.type != WLR_INPUT_DEVICE_POINTER ||
        !wlr_input_device_is_libinput(&pointer->base)) {
        return false;
    }

    struct libinput_device *libinput_device =
        wlr_libinput_get_device_handle(&pointer->base);

    return libinput_device_config_tap_get_finger_count(libinput_device) > 0;
}

// set the cursor libinput configuration
void Cursor::set_config(wlr_pointer *pointer) {
    const Config *config = server->config;

    if (!wlr_input_device_is_libinput(&pointer->base))
        return;
    libinput_device *device = wlr_libinput_get_device_handle(&pointer->base);

    if (is_touchpad(pointer)) {
        // touchpad config

        if (libinput_device_config_tap_get_finger_count(device)) {
            // tap to click
            libinput_device_config_tap_set_enabled(
                device, config->cursor.touchpad.tap_to_click);

            // tap and drag
            libinput_device_config_tap_set_drag_enabled(
                device, config->cursor.touchpad.tap_and_drag);

            // drag lock
            libinput_device_config_tap_set_drag_lock_enabled(
                device, config->cursor.touchpad.drag_lock);

            // button map
            libinput_device_config_tap_set_button_map(
                device, config->cursor.touchpad.tap_button_map);
        }

        // natural scroll
        if (libinput_device_config_scroll_has_natural_scroll(device))
            libinput_device_config_scroll_set_natural_scroll_enabled(
                device, config->cursor.touchpad.natural_scroll);

        // disable while typing
        if (libinput_device_config_dwt_is_available(device))
            libinput_device_config_dwt_set_enabled(
                device, config->cursor.touchpad.disable_while_typing);

        // left-handed
        if (libinput_device_config_left_handed_is_available(device))
            libinput_device_config_left_handed_set(
                device, config->cursor.touchpad.left_handed);

        // middle emulation
        if (libinput_device_config_middle_emulation_is_available(device))
            libinput_device_config_middle_emulation_set_enabled(
                device, config->cursor.touchpad.middle_emulation);

        // scroll method
        if (libinput_device_config_scroll_get_methods(device) !=
            LIBINPUT_CONFIG_SCROLL_NO_SCROLL)
            libinput_device_config_scroll_set_method(
                device, config->cursor.touchpad.scroll_method);

        // click method
        if (libinput_device_config_click_get_methods(device) !=
            LIBINPUT_CONFIG_CLICK_METHOD_NONE)
            libinput_device_config_click_set_method(
                device, config->cursor.touchpad.click_method);

        // event mode
        if (libinput_device_config_send_events_get_modes(device))
            libinput_device_config_send_events_set_mode(
                device, config->cursor.touchpad.event_mode);

        if (libinput_device_config_accel_is_available(device)) {
            // profile
            libinput_device_config_accel_set_profile(
                device, config->cursor.touchpad.profile);

            // accel speed
            libinput_device_config_accel_set_speed(
                device, config->cursor.touchpad.accel_speed);
        }
    } else {
        // mouse config

        // natural scroll
        if (libinput_device_config_scroll_has_natural_scroll(device))
            libinput_device_config_scroll_set_natural_scroll_enabled(
                device, config->cursor.mouse.natural_scroll);

        // left-handed
        if (libinput_device_config_left_handed_is_available(device))
            libinput_device_config_left_handed_set(
                device, config->cursor.mouse.left_handed);

        if (libinput_device_config_accel_is_available(device)) {
            // profile
            libinput_device_config_accel_set_profile(
                device, config->cursor.mouse.profile);

            // accel speed
            libinput_device_config_accel_set_speed(
                device, config->cursor.mouse.accel_speed);
        }
    }

    // add pointer to list
    if (std::find(pointers.begin(), pointers.end(), pointer) == pointers.end())
        pointers.push_back(pointer);
}

// reconfigure all pointers
void Cursor::reconfigure_all() {
    for (wlr_pointer *pointer : pointers)
        set_config(pointer);
}
