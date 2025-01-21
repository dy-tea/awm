#include <wayland-client-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>
#include <unistd.h>
#include "Server.h"
#include "Popup.h"
#include "wlr.h"

Server::Server() {
    display = wl_display_create();
    backend = wlr_backend_autocreate(wl_display_get_event_loop(display), NULL);
    if (backend == NULL) {
        wlr_log(WLR_ERROR, "Failed to create backend");
        exit(1);
    }

    renderer = wlr_renderer_autocreate(backend);
    if (renderer == NULL) {
        wlr_log(WLR_ERROR, "Failed to create renderer");
        exit(1);
    }
    wlr_renderer_init_wl_display(renderer, display);

    allocator = wlr_allocator_autocreate(backend, renderer);
    if (allocator == NULL) {
        wlr_log(WLR_ERROR, "Failed to create allocator");
        exit(1);
    }

    wlr_compositor_create(display, 5, renderer);
    wlr_subcompositor_create(display);
    wlr_data_device_manager_create(display);

    output_layout = wlr_output_layout_create(display);
    output_new.notify = [](wl_listener *listener, void *data) {
        wlr_output *out = (wlr_output*)data;
        Server *server = wl_container_of(listener, server, output_new);

        wlr_output_init_render(out, server->allocator, server->renderer);

        wlr_output_state state;
        wlr_output_state_init(&state);
        wlr_output_state_set_enabled(&state, true);

        wlr_output_mode *mode = wlr_output_preferred_mode(out);
        if (mode != NULL)
            wlr_output_state_set_mode(&state, mode);

        wlr_output_commit_state(out, &state);
        wlr_output_state_finish(&state);

        Output *output = new Output();
        output->output = out;
        output->frame.notify = [](wl_listener *listener, void *data) {
           Output *output = wl_container_of(listener, output, request_state);
           wlr_output_event_request_state *event = (wlr_output_event_request_state*)data;
           wlr_output_commit_state(output->output, event->state);
        };
        wl_signal_add(&out->events.request_state, &output->request_state);
        output->destroy.notify = [](wl_listener *listener, void *data) {
            Output *output = wl_container_of(listener, output, destroy);
            wl_list_remove(&output->frame.link);
            wl_list_remove(&output->request_state.link);
            wl_list_remove(&output->destroy.link);
            delete output;
        };
        wl_signal_add(&out->events.destroy, &output->destroy);

        server->outputs.emplace_back(output);

        wlr_output_layout_output *l_output = wlr_output_layout_add_auto(server->output_layout, out);
        wlr_scene_output *scene_output = wlr_scene_output_create(server->scene, out);
        wlr_scene_output_layout_add_output(server->scene_layout, l_output, scene_output);
    };
    wl_signal_add(&backend->events.new_output, &output_new);

    scene = wlr_scene_create();
    scene_layout = wlr_scene_attach_output_layout(scene, output_layout);

    xdg_shell = wlr_xdg_shell_create(display, 3);
    new_xdg_toplevel.notify = [](wl_listener *listener, void *data) {
        Server *server = wl_container_of(listener, server, new_xdg_toplevel);
        wlr_xdg_toplevel *xdg_toplevel = (wlr_xdg_toplevel*)data;

        Toplevel *toplevel = new Toplevel();
        toplevel->xdg_toplevel = xdg_toplevel;
        toplevel->scene_tree = wlr_scene_xdg_surface_create(&server->scene->tree, xdg_toplevel->base);
        toplevel->scene_tree->node.data = toplevel;
        xdg_toplevel->base->data = toplevel->scene_tree;

        toplevel->map.notify = [](wl_listener *listener, void *data) {
            Toplevel *toplevel = wl_container_of(listener, toplevel, map);
            Server *server = wl_container_of(toplevel, server, toplevels);
            server->toplevels.emplace_back(toplevel);
            server->focus_toplevel(toplevel);
        };
        wl_signal_add(&xdg_toplevel->base->surface->events.map, &toplevel->map);
        toplevel->unmap.notify = [](wl_listener *listener, void *data) {
            Toplevel *toplevel = wl_container_of(listener, toplevel, unmap);
            Server *server = wl_container_of(toplevel, server, toplevels);
            if (toplevel == server->grabbed_toplevel)
                server->reset_cursor_mode();
            toplevel->link.clear();
        };
        wl_signal_add(&xdg_toplevel->base->surface->events.unmap, &toplevel->unmap);
        toplevel->commit.notify = [](wl_listener *listener, void *data) {
            Toplevel *toplevel = wl_container_of(listener, toplevel, commit);
            if (toplevel->xdg_toplevel->base->initial_commit)
                wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, 0, 0);
        };
        wl_signal_add(&xdg_toplevel->base->surface->events.commit, &toplevel->commit);

        toplevel->destroy.notify = [](wl_listener *listener, void *data) {
            Toplevel *toplevel = wl_container_of(listener, toplevel, destroy);
            wl_list_remove(&toplevel->map.link);
            wl_list_remove(&toplevel->unmap.link);
            wl_list_remove(&toplevel->commit.link);
            wl_list_remove(&toplevel->destroy.link);
            wl_list_remove(&toplevel->request_move.link);
            wl_list_remove(&toplevel->request_resize.link);
            wl_list_remove(&toplevel->request_maximise.link);
            wl_list_remove(&toplevel->request_fullscreen.link);
            delete toplevel;
        };
        wl_signal_add(&xdg_toplevel->events.destroy, &toplevel->destroy);

        toplevel->request_move.notify = [](wl_listener *listener, void *data) {
            Toplevel *toplevel = wl_container_of(listener, toplevel, request_move);
            Server *server = wl_container_of(toplevel, server, toplevels);
            server->begin_interactive(toplevel, move, 0);
        };
        wl_signal_add(&xdg_toplevel->events.request_move, &toplevel->request_move);
        toplevel->request_resize.notify = [](wl_listener *listener, void *data) {
            wlr_xdg_toplevel_resize_event *event = (wlr_xdg_toplevel_resize_event*)data;
            Toplevel *toplevel = wl_container_of(listener, toplevel, request_resize);
            Server *server = wl_container_of(toplevel, server, toplevels);
            server->begin_interactive(toplevel, resize, event->edges);
        };
        wl_signal_add(&xdg_toplevel->events.request_resize, &toplevel->request_resize);
        toplevel->request_maximise.notify = [](wl_listener *listener, void *data) {
            Toplevel *toplevel = wl_container_of(listener, toplevel, request_maximise);
            if (toplevel->xdg_toplevel->base->initialized)
                wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);
        };
        wl_signal_add(&xdg_toplevel->events.request_maximize, &toplevel->request_maximise);
        toplevel->request_fullscreen.notify = [](wl_listener *listener, void *data) {
            Toplevel *toplevel = wl_container_of(listener, toplevel, request_fullscreen);
            if (toplevel->xdg_toplevel->base->initialized)
                wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);
        };
        wl_signal_add(&xdg_toplevel->events.request_fullscreen, &toplevel->request_fullscreen);
    };
    wl_signal_add(&xdg_shell->events.new_toplevel, &new_xdg_toplevel);
    new_xdg_popup.notify = [](wl_listener *listener, void *data) {
        wlr_xdg_popup *xdg_popup = (wlr_xdg_popup*)data;
        Popup *popup = new Popup();
        popup->popup = xdg_popup;

        wlr_xdg_surface *parent = wlr_xdg_surface_try_from_wlr_surface(xdg_popup->parent);
        if (parent == NULL) {
            wlr_log(WLR_ERROR, "Failed to find parent xdg_surface");
            delete popup;
            return;
        }
        wlr_scene_tree *parent_tree = (wlr_scene_tree*)parent->data;
        xdg_popup->base->data = wlr_scene_xdg_surface_create(parent_tree, xdg_popup->base);

        popup->commit.notify = [](wl_listener *listener, void *data) {
            Popup *popup = wl_container_of(listener, popup, commit);
            if (popup->popup->base->initial_commit)
                wlr_xdg_surface_schedule_configure(popup->popup->base);
        };
        wl_signal_add(&xdg_popup->base->surface->events.commit, &popup->commit);

        popup->destroy.notify = [](wl_listener *listener, void *data) {
            Popup *popup = wl_container_of(listener, popup, destroy);
            wl_list_remove(&popup->commit.link);
            wl_list_remove(&popup->destroy.link);
            delete popup;
        };
        wl_signal_add(&xdg_popup->events.destroy, &popup->destroy);
    };
    wl_signal_add(&xdg_shell->events.new_popup, &new_xdg_popup);

    cursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(cursor, output_layout);

    cursor_mgr = wlr_xcursor_manager_create(NULL, 24);

    mode = passthrough;
    cursor_motion.notify = [](wl_listener *listener, void *data) {
        Server *server = wl_container_of(listener, server, cursor_motion);
        wlr_pointer_motion_event *event = (wlr_pointer_motion_event*)data;

        wlr_cursor_move(server->cursor, &event->pointer->base, event->delta_x, event->delta_y);
        server->process_cursor_motion(event->time_msec);
    };
    wl_signal_add(&cursor->events.motion, &cursor_motion);
    cursor_motion_absolute.notify = [](wl_listener *listener, void *data) {
        Server *server = wl_container_of(listener, server, cursor_motion_absolute);
        wlr_pointer_motion_absolute_event *event = (wlr_pointer_motion_absolute_event*)data;
        wlr_cursor_warp_absolute(server->cursor, &event->pointer->base, event->x, event->y);
        server->process_cursor_motion(event->time_msec);
    };
    wl_signal_add(&cursor->events.motion_absolute, &cursor_motion_absolute);
    cursor_button.notify = [](wl_listener *listener, void *data) {
        Server *server = wl_container_of(listener, server, cursor_button);
        wlr_pointer_button_event *event = (wlr_pointer_button_event*)data;

        wlr_seat_pointer_notify_button(server->seat, event->time_msec, event->button, event->state);
        if (event->state == WL_POINTER_BUTTON_STATE_RELEASED)
            server->reset_cursor_mode();
        else {
            double sx, sy;
            wlr_surface *surface = nullptr;
            Toplevel *toplevel = server->desktop_toplevel_at(server->cursor->x, server->cursor->y, &surface, &sx, &sy);
            server->focus_toplevel(toplevel);
        }
    };
    wl_signal_add(&cursor->events.button, &cursor_button);
    cursor_axis.notify = [](wl_listener *listener, void *data) {
        Server *server = wl_container_of(listener, server, cursor_axis);
        wlr_pointer_axis_event *event = (wlr_pointer_axis_event*)data;
        wlr_seat_pointer_notify_axis(server->seat, event->time_msec, event->orientation, event->delta, event->delta_discrete, event->source, event->relative_direction);
    };
    wl_signal_add(&cursor->events.axis, &cursor_axis);
    cursor_frame.notify = [](wl_listener *listener, void *data) {
        Server *server = wl_container_of(listener, server, cursor_frame);
        wlr_seat_pointer_notify_frame(server->seat);
    };
    wl_signal_add(&cursor->events.frame, &cursor_frame);

    keyboards = std::vector<Keyboard*>();
    new_input.notify = [](wl_listener *listener, void *data) {
        Server *server = wl_container_of(listener, server, new_input);
        wlr_input_device *device = (wlr_input_device*)data;
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

        uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
        if (server->keyboards.size())
            caps |= WL_SEAT_CAPABILITY_KEYBOARD;
        wlr_seat_set_capabilities(server->seat, caps);
    };
    wl_signal_add(&backend->events.new_input, &new_input);
    seat = wlr_seat_create(display, "seat0");
    request_cursor.notify = [](wl_listener *listener, void *data) {
        Server *server = wl_container_of(listener, server, request_cursor);
        wlr_seat_pointer_request_set_cursor_event *event = (wlr_seat_pointer_request_set_cursor_event*)data;
        wlr_seat_client *focused_client = server->seat->pointer_state.focused_client;

        if (focused_client == event->seat_client)
            wlr_cursor_set_surface(server->cursor, event->surface, event->hotspot_x, event->hotspot_y);
    };
    wl_signal_add(&seat->events.request_set_cursor, &request_cursor);

    const char *socket = wl_display_add_socket_auto(display);
    if (!socket) {
        wlr_backend_destroy(backend);
        exit(1);
    }
    if (!wlr_backend_start(backend)) {
        wlr_backend_destroy(backend);
        wl_display_destroy(display);
        exit(1);
    }

    setenv("WAYLAND_DISPLAY", socket, true);

    if (fork() == 0) {
        execl("/bin/sh", "/bin/sh", "-c", "foot", (void *)NULL);
    }

    wlr_log(WLR_INFO, "Running on WAYLAND_DISPLAY=%s", socket);
    wl_display_run(display);
}

Server::~Server() {
    wl_display_destroy_clients(display);

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

	wl_list_remove(&output_new.link);

	wlr_scene_node_destroy(&scene->tree.node);
	wlr_xcursor_manager_destroy(cursor_mgr);
	wlr_cursor_destroy(cursor);
	wlr_allocator_destroy(allocator);
	wlr_renderer_destroy(renderer);
	wlr_backend_destroy(backend);
	wl_display_destroy(display);
}

void Server::focus_toplevel(Toplevel *toplevel) {
    if (toplevel == NULL) return;

    wlr_surface *prev_surface = seat->keyboard_state.focused_surface;
    wlr_surface *surface = toplevel->xdg_toplevel->base->surface;
    if (prev_surface == surface) return;
    if (prev_surface) {
        wlr_xdg_toplevel *prev_toplevel = wlr_xdg_toplevel_try_from_wlr_surface(prev_surface);
        if (prev_toplevel != NULL)
            wlr_xdg_toplevel_set_activated(prev_toplevel, false);
    }
    wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);

    wlr_scene_node_raise_to_top(&toplevel->scene_tree->node);
    toplevel->link.clear();
    toplevels.emplace_back(toplevel);
    wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, true);

    if (keyboard != NULL)
        wlr_seat_keyboard_notify_enter(seat, surface, keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
}

bool Server::handle_keybinding(xkb_keysym_t sym) {
    switch (sym) {
    case XKB_KEY_Escape:
        wl_display_terminate(display);
        break;
    case XKB_KEY_F1: {
        if (toplevels.size() < 2) break;

        wlr_surface *prev_surface = seat->keyboard_state.focused_surface;
        if (prev_surface == NULL) return false;
        wlr_xdg_toplevel *prev_toplevel = wlr_xdg_toplevel_try_from_wlr_surface(prev_surface);

        bool flag = false;
        for (Toplevel *t : toplevels) {
            if (flag) {
                focus_toplevel(t);
                break;
            }
            if (t->xdg_toplevel == prev_toplevel)
                flag = true;
        }
        if (!flag)
            focus_toplevel(toplevels[0]);

        break;
    }
    default:
        return false;
    }
    return true;
}

void Server::new_keyboard(wlr_input_device *device) {
    wlr_keyboard *wlr_keyboard = wlr_keyboard_from_input_device(device);
    Keyboard *keyboard = new Keyboard();
    keyboard->keyboard = *wlr_keyboard;

    xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    xkb_keymap *keymap = xkb_keymap_new_from_names(context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);

    wlr_keyboard_set_keymap(wlr_keyboard, keymap);
    xkb_keymap_unref(keymap);
    xkb_context_unref(context);
    wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);

    keyboard->modifiers.notify = [](wl_listener *listener, void *data) {
        Keyboard *keyboard = wl_container_of(listener, keyboard, modifiers);
        Server *server = wl_container_of(listener, server, keyboards);
        wlr_seat_set_keyboard(server->seat, &keyboard->keyboard);
        wlr_seat_keyboard_notify_modifiers(server->seat, &keyboard->keyboard.modifiers);
    };
    wl_signal_add(&wlr_keyboard->events.modifiers, &keyboard->modifiers);
    keyboard->key.notify = [](wl_listener *listener, void *data) {
        Keyboard *keyboard = wl_container_of(listener, keyboard, key);
        Server *server = wl_container_of(listener, server, keyboards);
        wlr_keyboard_key_event *event = (wlr_keyboard_key_event*)data;
        wlr_seat *seat = server->seat;

        uint32_t keycode = event->keycode + 8;
        const xkb_keysym_t *syms;
        int nsyms = xkb_state_key_get_syms(keyboard->keyboard.xkb_state, keycode, &syms);

        bool handled = false;
        uint32_t modifiers = wlr_keyboard_get_modifiers(&keyboard->keyboard);
        if ((modifiers & WLR_MODIFIER_ALT) && event->state == WL_KEYBOARD_KEY_STATE_PRESSED)
            for (int i = 0; i != nsyms; ++i)
                handled = server->handle_keybinding(syms[i]);

        if (!handled) {
            wlr_seat_set_keyboard(seat, &keyboard->keyboard);
            wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode, event->state);
        }
    };
}

void Server::new_pointer(wlr_input_device *device) {
    wlr_cursor_attach_input_device(cursor, device);
}

Toplevel *Server::desktop_toplevel_at(double lx, double ly, wlr_surface **surface, double *sx, double *sy) {
    wlr_scene_node *node = wlr_scene_node_at(&scene->tree.node, lx, ly, sx, sy);
    if (node == NULL || node->type != WLR_SCENE_NODE_BUFFER)
        return nullptr;

    wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
    wlr_scene_surface *scene_surface = wlr_scene_surface_try_from_buffer(scene_buffer);
    if (!scene_surface)
        return nullptr;

    *surface = scene_surface->surface;

    wlr_scene_tree *tree = node->parent;
    while (tree != NULL && tree->node.data == NULL)
        tree = tree->node.parent;

    return (Toplevel*)tree->node.data;
}

void Server::reset_cursor_mode() {
    mode = passthrough;
    grabbed_toplevel = NULL;
}

void Server::process_cursor_move() {
    wlr_scene_node_set_position(&grabbed_toplevel->scene_tree->node, cursor->x - grab_x, cursor->y - grab_y);
}

void Server::process_cursor_resize() {
    double bx = cursor->x - grab_x;
    double by = cursor->y - grab_y;
    int nl = grab_geobox.x;
    int nr = grab_geobox.x + grab_geobox.width;
    int nt = grab_geobox.y;
    int nb = grab_geobox.y + grab_geobox.height;

    if (resize_edges & WLR_EDGE_TOP) {
        nt = by;
        if (nt >= nb)
            nt = nb - 1;
    } else if (resize_edges & WLR_EDGE_BOTTOM) {
        nb = by;
        if (nb <= nt)
            nb = nt + 1;
    }
    if (resize_edges & WLR_EDGE_LEFT) {
        nl = bx;
        if (nl >= nr)
            nl = nr - 1;
    } else if (resize_edges & WLR_EDGE_RIGHT) {
        nr = bx;
        if (nr <= nl)
            nr = nl + 1;
    }

    wlr_box *geo_box = &grabbed_toplevel->xdg_toplevel->base->geometry;
    wlr_scene_node_set_position(&grabbed_toplevel->scene_tree->node, nl - geo_box->x, nt - geo_box->y);
    wlr_xdg_toplevel_set_size(grabbed_toplevel->xdg_toplevel, nr - nl, nb - nt);
}

void Server::process_cursor_motion(uint32_t time) {
    if (mode == move) {
        process_cursor_move();
        return;
    } else if (mode == resize) {
        process_cursor_resize();
        return;
    }

    double sx, sy;
    wlr_surface *surface = nullptr;
    Toplevel *toplevel = desktop_toplevel_at(cursor->x, cursor->y, &surface, &sx, &sy);

    if (!toplevel)
        wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");

    if (surface) {
        wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
        wlr_seat_pointer_notify_motion(seat, time, sx, sy);
    } else wlr_seat_pointer_clear_focus(seat);
}

void Server::begin_interactive(Toplevel *toplevel, cursor_mode mode, uint32_t edges) {
    grabbed_toplevel = toplevel;
    this->mode = mode;

    if (mode == move) {
        grab_x = cursor->x - toplevel->scene_tree->node.x;
        grab_y = cursor->y - toplevel->scene_tree->node.y;
    } else {
        wlr_box *geo_box = &toplevel->xdg_toplevel->base->geometry;
        double bx = (toplevel->scene_tree->node.x + geo_box->x) + ((edges & WLR_EDGE_RIGHT) ? geo_box->width : 0);
        double by = (toplevel->scene_tree->node.y + geo_box->y) + ((edges & WLR_EDGE_BOTTOM) ? geo_box->height : 0);
        grab_x = cursor->x - bx;
        grab_y = cursor->y - by;

        grab_geobox = *geo_box;
        grab_geobox.x += toplevel->scene_tree->node.x;
        grab_geobox.y += toplevel->scene_tree->node.y;

        resize_edges = edges;
    }
}
