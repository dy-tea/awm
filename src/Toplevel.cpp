#include "Toplevel.h"
#include "BSPTree.h"
#include "Config.h"
#include "IPC.h"
#include "Output.h"
#include "OutputManager.h"
#include "Popup.h"
#include "Seat.h"
#include "Server.h"
#include "WindowRule.h"
#include "Workspace.h"
#include "wlr/types/wlr_xdg_decoration_v1.h"

void Toplevel::map_notify(wl_listener *listener, [[maybe_unused]] void *data) {
    // on map or display
    Toplevel *toplevel = wl_container_of(listener, toplevel, map);

    Server *server = toplevel->server;
    Output *output = nullptr;

    // update pid
    toplevel->update_pid();

    // apply rule
    WindowRule *matching_rule = nullptr;
    for (WindowRule *rule : server->config->window_rules)
        if (rule->matches(toplevel)) {
            matching_rule = rule;
            output = server->output_manager->get_output(matching_rule->output);
            break;
        }

    if (!output)
        output = server->focused_output();

    // set activation token if found
    if (!toplevel->activation_token) {
        toplevel->activation_token =
            server->find_activation_token(toplevel->pid);
        if (toplevel->activation_token)
            toplevel->activation_token->owning_toplevel = toplevel;
    }

    // xdg toplevel
    if (const wlr_xdg_toplevel *xdg_toplevel = toplevel->xdg_toplevel) {
        wlr_surface *base = xdg_toplevel->base->surface;

        // set the fractional scale for this surface
        const float scale = output->wlr_output->scale;
        wlr_fractional_scale_v1_notify_scale(base, scale);
        wlr_surface_set_preferred_buffer_scale(base, ceil(scale));

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
            width = base->current.width;
            height = base->current.height;
        }

        // ensure size does not exceed output
        width = std::min(width, static_cast<uint32_t>(usable_area.width));
        height = std::min(height, static_cast<uint32_t>(usable_area.height));

        Workspace *workspace = output->get_active();
        if (!matching_rule && workspace && workspace->auto_tile) {
            toplevel->geometry.width = width;
            toplevel->geometry.height = height;

            // hide the scene node until tile() positions it
            wlr_scene_node_set_enabled(&toplevel->scene_tree->node, false);
            toplevel->scene_hidden_for_autotile = true;
        } else {
            wlr_box output_box = output->layout_geometry;

            int32_t x = output_box.x + usable_area.x + (usable_area.width - width) / 2;
            int32_t y = output_box.y + usable_area.y + (usable_area.height - height) / 2;

            // ensure position falls in bounds
            x = std::max(x, output_box.x + usable_area.x);
            y = std::max(y, output_box.y + usable_area.y);

            // set position and size
            toplevel->set_position_size(x, y, width, height);
        }

        // create image capture scene surface
        toplevel->image_capture_surface =
            wlr_scene_surface_create(&toplevel->image_capture->tree, base);
    }
#ifdef XWAYLAND
    // xwayland surface
    else {
        wlr_xwayland_surface *xwayland_surface = toplevel->xwayland_surface;
        if (!xwayland_surface->surface)
            return;

        wlr_surface *base = xwayland_surface->surface;

        // add commit listener
        toplevel->commit.notify = [](wl_listener *listener,
                                     [[maybe_unused]] void *data) {
            Toplevel *toplevel = wl_container_of(listener, toplevel, commit);
            wlr_xwayland_surface *xwayland_surface = toplevel->xwayland_surface;
            if (!xwayland_surface->surface)
                return;

            wlr_surface_state *state = &xwayland_surface->surface->current;
            wlr_box *current = &toplevel->geometry;

            // update size if state differs
            if (current->width != state->width ||
                current->height != state->height) {
                current->width = state->width;
                current->height = state->height;
                toplevel->set_position_size(*current);
            }
        };
        wl_signal_add(&base->events.commit, &toplevel->commit);

        // create scene surface
        toplevel->scene_surface =
            wlr_scene_surface_create(toplevel->scene_tree, base);

        // create image capture scene surface
        toplevel->image_capture_surface =
            wlr_scene_surface_create(&toplevel->image_capture->tree, base);

        // get usable area of the output
        wlr_box area = output->usable_area;

        // calculate the width and height
        int width = std::min((int)xwayland_surface->width, area.width);
        int height = std::min((int)xwayland_surface->height, area.height);

        Workspace *workspace = output->get_active();
        if (!matching_rule && workspace && workspace->auto_tile) {
            toplevel->geometry.x = 0;
            toplevel->geometry.y = 0;
            toplevel->geometry.width = width;
            toplevel->geometry.height = height;

            // hide the scene node until tile() positions it
            wlr_scene_node_set_enabled(&toplevel->scene_tree->node, false);
            toplevel->scene_hidden_for_autotile = true;
        } else {
            // get surface position
            int x = xwayland_surface->x;
            int y = xwayland_surface->y;

            // center the surface to the focused output if zero
            if (!x)
                x = output->layout_geometry.x + area.x + (area.width - width) / 2;
            if (!y)
                y = output->layout_geometry.y + area.y + (area.height - height) / 2;

            // send a configure event
            wlr_xwayland_surface_configure(xwayland_surface, x, y, width,
                                           height);

            // set the position
            wlr_scene_node_set_position(&toplevel->scene_surface->buffer->node,
                                        x, y);

            // set initial geometry
            toplevel->geometry.x = x;
            toplevel->geometry.y = y;
            toplevel->geometry.width = width;
            toplevel->geometry.height = height;
        }

        // add the toplevel to the scene tree
        toplevel->scene_surface->buffer->node.data = toplevel;
        toplevel->scene_tree->node.data = toplevel;

        // set seat if requested
        if (wlr_xwayland_surface_override_redirect_wants_focus(
                xwayland_surface))
            wlr_xwayland_set_seat(server->xwayland, server->seat->wlr_seat);
    }
#endif

    // foreign toplevel
    toplevel->create_foreign();
    toplevel->create_ext_foreign();

    if (matching_rule) {
        // apply window rule
        matching_rule->apply(toplevel);
    } else {
        // set floating state based on size constraints if no rule matches
        toplevel->is_floating = toplevel->should_be_floating();
        // add toplevel to active workspace and focus it
        output->get_active()->add_toplevel(toplevel, true);
    }

    // set app id for foreign toplevel
    if (std::string app_id = std::string(toplevel->get_app_id());
        !app_id.empty()) {
        wlr_foreign_toplevel_handle_v1_set_app_id(toplevel->foreign_handle,
                                                  app_id.c_str());
    }

    toplevel->update_ext_foreign();

    // notify clients
    if (toplevel->server->ipc)
        toplevel->server->ipc->notify_clients(
            {IPC_TOPLEVEL_LIST, IPC_WORKSPACE_LIST});
}

void Toplevel::unmap_notify(wl_listener *listener,
                            [[maybe_unused]] void *data) {
    Toplevel *toplevel = wl_container_of(listener, toplevel, unmap);
    Server *server = toplevel->server;

    // deactivate
    if (toplevel == server->seat->grabbed_toplevel)
        server->cursor->reset_mode();

    // remove from workspace
    if (Workspace *workspace = server->get_workspace(toplevel))
        workspace->close(toplevel);

    // remove links
    wl_list_remove(&toplevel->link);

#ifdef XWAYLAND
    // commit is per-surface with xwayland
    if (toplevel->xwayland_surface)
        wl_list_remove(&toplevel->commit.link);
#endif

    // remove foreign handle
    if (toplevel->foreign_handle) {
        wl_list_remove(&toplevel->foreign_activate.link);
        wl_list_remove(&toplevel->foreign_close.link);
        wlr_foreign_toplevel_handle_v1_destroy(toplevel->foreign_handle);
    }

    // remove ext foreign handle
    if (toplevel->ext_foreign_handle) {
        wl_list_remove(&toplevel->ext_foreign_destroy.link);
        wlr_ext_foreign_toplevel_handle_v1_destroy(
            toplevel->ext_foreign_handle);
    }

    // remove reference
    toplevel->image_capture_surface = nullptr;

    // notify clients
    if (IPC *ipc = server->ipc)
        ipc->notify_clients({IPC_TOPLEVEL_LIST, IPC_WORKSPACE_LIST});
}

// Toplevel from xdg toplevel
Toplevel::Toplevel(Server *server, wlr_xdg_toplevel *xdg_toplevel)
    : server(server), xdg_toplevel(xdg_toplevel) {
    // add the toplevel to the scene tree
    scene_tree = wlr_scene_xdg_surface_create(server->layers.floating,
                                              xdg_toplevel->base);
    image_capture = wlr_scene_create();
    image_capture_tree = wlr_scene_tree_create(&image_capture->tree);
    scene_tree->node.data = this;
    xdg_toplevel->base->data = this;

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

        if (toplevel->xdg_toplevel->base->initial_commit) {
            // let client pick dimensions
            wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, 0, 0);

            // set capabilities
            wlr_xdg_toplevel_set_wm_capabilities(
                toplevel->xdg_toplevel,
                WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN |
                    WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE |
                    WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE);
        }
    };
    wl_signal_add(&xdg_toplevel->base->surface->events.commit, &commit);

    // new_xdg_popup
    new_xdg_popup.notify = [](wl_listener *listener, void *data) {
        Toplevel *toplevel = wl_container_of(listener, toplevel, new_xdg_popup);

        // popups do not need to be tracked
        new Popup(static_cast<wlr_xdg_popup *>(data), toplevel->scene_tree,
                  toplevel->image_capture_tree, toplevel->server);
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

        // weston-simple-dmabuf-feedback aborts the server without this check
        if (!toplevel->xdg_toplevel->base->initialized)
            return;

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
        Server *server = toplevel->server;

        if (int64_t to = server->config->general.minimize_to_workspace) {
            // move toplevel to workspace
            if (Workspace *workspace = server->get_workspace(toplevel))
                workspace->move_to(toplevel,
                                   workspace->output->get_workspace(to));
        } else {
            // move toplevel to the bottom
            wlr_scene_node_lower_to_bottom(&toplevel->scene_tree->node);

            // focus the next toplevel in the workspace
            if (Workspace *workspace = server->get_workspace(toplevel))
                workspace->focus_next();
        }
    };
    wl_signal_add(&xdg_toplevel->events.request_minimize, &request_minimize);

    // set_title
    set_title.notify = []([[maybe_unused]] wl_listener *listener,
                          [[maybe_unused]] void *data) {
        // noop
    };
    wl_signal_add(&xdg_toplevel->events.set_title, &set_title);

    // set_app_id
    set_app_id.notify = []([[maybe_unused]] wl_listener *listener,
                           [[maybe_unused]] void *data) {
        // noop
    };
    wl_signal_add(&xdg_toplevel->events.set_app_id, &set_app_id);
}

Toplevel::~Toplevel() {
#ifdef XWAYLAND
    if (xwayland_surface) {
        wl_list_remove(&activate.link);
        wl_list_remove(&associate.link);
        wl_list_remove(&dissociate.link);
        wl_list_remove(&configure.link);
        wl_list_remove(&focus_in.link);
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
        wl_list_remove(&set_title.link);
        wl_list_remove(&set_app_id.link);
#ifdef XWAYLAND
    }
#endif

    if (decoration) {
        delete decoration;
        decoration = nullptr;
    }

    if (activation_token) {
        activation_token->owning_toplevel = nullptr;
        delete activation_token;
    }

    if (!server->shutting_down) {
#ifdef XWAYLAND
        if (xwayland_surface && scene_tree) {
            wlr_scene_node_destroy(&scene_tree->node);
            scene_tree = nullptr;
        }
#endif

        if (image_capture) {
            wlr_scene_node_destroy(&image_capture->tree.node);
            image_capture = nullptr;
        }
    }

    wl_list_remove(&destroy.link);
}

#ifdef XWAYLAND
// Toplevel from xwayland surface
Toplevel::Toplevel(Server *server, wlr_xwayland_surface *xwayland_surface)
    : server(server), xwayland_surface(xwayland_surface) {

    // create scene tree
    scene_tree = wlr_scene_tree_create(server->layers.floating);
    scene_tree->node.data = this;

    // create capture
    image_capture = wlr_scene_create();
    image_capture_tree = wlr_scene_tree_create(&image_capture->tree);

    xwayland_surface->data = this;

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
    };
    wl_signal_add(&xwayland_surface->events.dissociate, &dissociate);

    // configure
    configure.notify = [](wl_listener *listener, void *data) {
        Toplevel *toplevel = wl_container_of(listener, toplevel, configure);

        const auto *event =
            static_cast<wlr_xwayland_surface_configure_event *>(data);
        wlr_xwayland_surface *xsurface = toplevel->xwayland_surface;

        // unconfigured
        if (!toplevel->xwayland_surface->surface ||
            !toplevel->xwayland_surface->surface->mapped) {
            wlr_xwayland_surface_configure(toplevel->xwayland_surface, event->x,
                                           event->y, event->width,
                                           event->height);
            return;
        }

        // unmanaged
        if (xsurface->override_redirect) {
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

    // focus in
    focus_in.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        Toplevel *toplevel = wl_container_of(listener, toplevel, focus_in);
        wlr_xwayland_surface *xsurface = toplevel->xwayland_surface;

        if (!xsurface->surface)
            return;

        if (toplevel != toplevel->server->seat->grabbed_toplevel)
            toplevel->focus();
    };
    wl_signal_add(&xwayland_surface->events.focus_in, &focus_in);

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

    wlr_seat *seat = server->seat->wlr_seat;
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
    server->seat->grabbed_toplevel = this;

    Cursor *cursor = server->cursor;
    cursor->cursor_mode = mode;
    cursor->grab_source_workspace = server->get_workspace(this);

    if (mode == CURSORMODE_MOVE) {
        wlr_scene_node_raise_to_top(&scene_tree->node);

        cursor->move_original_geo = geometry;

        if (maximized()) {
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
        if (fullscreen())
            return;

        // get toplevel geometry
        const wlr_box geo_box = get_geometry();

        cursor->resize_original_geo = geometry;

        // Find and store adjacent neighbors for live resize updates
        cursor->resize_neighbors.clear();
        cursor->resize_all_original_geometries.clear();
        if (Workspace *workspace = server->get_workspace(this)) {
            if (workspace->auto_tile && !workspace->bsp_tree) {
                const int ADJACENT_THRESHOLD = 10;

                // store original geometries for cascading
                Toplevel *tl, *tmp_tl;
                wl_list_for_each_safe(tl, tmp_tl, &workspace->toplevels, link)
                    cursor->resize_all_original_geometries[tl] = tl->geometry;

                // find direct neighbors
                Toplevel *neighbor, *tmp;
                wl_list_for_each_safe(neighbor, tmp, &workspace->toplevels,
                                      link) {
                    if (neighbor == this || neighbor->fullscreen())
                        continue;

                    ResizeNeighbor rn;
                    rn.toplevel = neighbor;
                    rn.original_geo = neighbor->geometry;

                    // check adjacency to all four edges
                    if (std::abs(rn.original_geo.x -
                                 (geometry.x + geometry.width)) <
                        ADJACENT_THRESHOLD)
                        rn.adjacent_right = true;

                    if (std::abs((rn.original_geo.x + rn.original_geo.width) -
                                 geometry.x) < ADJACENT_THRESHOLD)
                        rn.adjacent_left = true;

                    if (std::abs(rn.original_geo.y -
                                 (geometry.y + geometry.height)) <
                        ADJACENT_THRESHOLD)
                        rn.adjacent_bottom = true;

                    if (std::abs((rn.original_geo.y + rn.original_geo.height) -
                                 geometry.y) < ADJACENT_THRESHOLD)
                        rn.adjacent_top = true;

                    // only store if adjacent to at least one edge
                    if (rn.adjacent_right || rn.adjacent_left ||
                        rn.adjacent_bottom || rn.adjacent_top) {
                        cursor->resize_neighbors.push_back(rn);
                    }
                }
            }
        }

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
void Toplevel::set_position_size(double x, double y, int width, int height) {
    // xwayland surfaces can call fullscreen and maximize when unmapped so this
    // check is necessary
#ifdef XWAYLAND
    if (xwayland_surface && !xwayland_surface->surface->mapped)
        return;
#endif

    // enforce minimum size
    // if width and height are 0, it is likely a virtual output
    if (width && height
#ifdef XWAYLAND
        && xdg_toplevel
#endif
    ) {
        width = std::max(width, xdg_toplevel->current.min_width);
        height = std::max(height, xdg_toplevel->current.min_height);
    }

    // toggle maximized if maximized
    if (maximized()) {
#ifdef XWAYLAND
        if (xdg_toplevel)
#endif
            wlr_xdg_toplevel_set_maximized(xdg_toplevel, false);
#ifdef XWAYLAND
        else if (xwayland_surface)
            wlr_xwayland_surface_set_maximized(xwayland_surface, false, false);
#endif
    }

    if (fullscreen()) {
        // set toplevel window mode to not fullscreen
        if (xdg_toplevel)
            wlr_xdg_toplevel_set_fullscreen(xdg_toplevel, false);
#ifdef XWAYLAND
        else
            wlr_xwayland_surface_set_fullscreen(xwayland_surface, false);
#endif

        // show decoration
        if (decoration &&
            decoration_mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE)
            decoration->set_visible(true);

        // move scene tree node to toplevel tree
        wlr_scene_node_reparent(&scene_tree->node, server->layers.floating);
    }

    // store the original geometry (without decoration offset) for later use
    geometry = wlr_box{static_cast<int>(x), static_cast<int>(y), width, height};

    // re-enable scene node if it was disabled for auto-tile positioning
    if (scene_hidden_for_autotile) {
        wlr_scene_node_set_enabled(&scene_tree->node, true);
        scene_hidden_for_autotile = false;
    }

    // apply decoration offset if decoration is visible
    if (decoration &&
        decoration_mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE &&
        decoration->visible) {
        int deco_height = 30;
        y += deco_height;
        height -= deco_height;
    }

    // update geometry
#ifdef XWAYLAND
    if (xdg_toplevel) {
#endif
        // set new position
        wlr_scene_node_set_position(&scene_tree->node, x, y);

        // set position and size
        wlr_xdg_toplevel_set_size(xdg_toplevel, width, height);

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

    if (decoration)
        decoration->update_titlebar(geometry.width);

    // notify clients
    if (IPC *ipc = server->ipc)
        ipc->notify_clients({IPC_TOPLEVEL_LIST, IPC_WORKSPACE_LIST});
}

void Toplevel::set_position_size(const wlr_box &geometry) {
    set_position_size(geometry.x, geometry.y, geometry.width, geometry.height);
}

void Toplevel::set_decoration_mode(wlr_xdg_toplevel_decoration_v1_mode mode) {
    if (fullscreen()) {
        decoration_mode = WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_NONE;
        return;
    }

    decoration_mode = mode;
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

    if (server->ipc)
        server->ipc->notify_clients(IPC_TOPLEVEL_LIST);
}

// returns true if the toplevel is maximized
bool Toplevel::maximized() const {
#ifdef XWAYLAND
    if (xdg_toplevel)
#endif
        return xdg_toplevel->current.maximized;
#ifdef XWAYLAND
    else
        return xwayland_maximized;
#endif
}

// returns true if the toplevel is fullscreen
bool Toplevel::fullscreen() const {
#ifdef XWAYLAND
    if (xdg_toplevel)
#endif
        return xdg_toplevel->current.fullscreen;
#ifdef XWAYLAND
    else
        return xwayland_surface->fullscreen;
#endif
};

// set the toplevel to be fullscreened
void Toplevel::set_fullscreen(const bool fullscreen) {
    // get output from toplevel's current workspace, fallback to focused output
    Output *output = nullptr;
    Workspace *workspace = server->get_workspace(this);
    if (workspace)
        output = workspace->output;
    else {
        output = server->focused_output();
        workspace = server->workspace_manager->get_active_workspace(output);
    }

    // get output geometry
    const wlr_box output_box = output->layout_geometry;

    // set toplevel window mode to fullscreen
    if (xdg_toplevel)
        wlr_xdg_toplevel_set_fullscreen(xdg_toplevel, fullscreen);
#ifdef XWAYLAND
    else
        wlr_xwayland_surface_set_fullscreen(xwayland_surface, fullscreen);
#endif

    if (fullscreen) {
        save_geometry();

        if (decoration &&
            decoration_mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE)
            decoration->set_visible(false);

        // move scene tree node to fullscreen tree
        wlr_scene_node_raise_to_top(&scene_tree->node);
        wlr_scene_node_reparent(&scene_tree->node, server->layers.fullscreen);

        // set to top left of output, width and height the size of output
        set_position_size(output_box.x, output_box.y, output_box.width,
                          output_box.height);
    } else {
        // show decoration BEFORE calling set_position_size
        if (decoration &&
            decoration_mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE)
            decoration->set_visible(true);

        // move scene tree node to toplevel tree
        wlr_scene_node_reparent(&scene_tree->node, server->layers.floating);

        // raise to top to maintain focus order when transitioning from
        // fullscreen
        wlr_scene_node_raise_to_top(&scene_tree->node);

        // check if we need to handle auto-tiling
        // Always return to auto-tile when unfullscreening, even if maximized
        bool should_auto_tile = workspace && workspace->auto_tile;

        if (should_auto_tile && workspace->bsp_tree) {
            // Un-maximize if currently maximized
            if (maximized()) {
#ifdef XWAYLAND
                if (xdg_toplevel)
#endif
                    wlr_xdg_toplevel_set_maximized(xdg_toplevel, false);
#ifdef XWAYLAND
                else if (xwayland_surface) {
                    wlr_xwayland_surface_set_maximized(xwayland_surface, false,
                                                       false);
                    xwayland_maximized = false;
                }
#endif
            }

            // re-insert into BSP tree if it's not already there
            if (!workspace->bsp_tree->find_node(this)) {
                workspace->bsp_tree->insert(this);
            }

            // apply BSP tree layout
            wlr_box new_geometry;
            if (workspace->bsp_tree->get_toplevel_geometry(
                    this, output->usable_area, new_geometry)) {
                set_position_size(new_geometry);
            } else {
                // fallback to saved geometry if BSP tree fails
                if (saved_geometry.width && saved_geometry.height)
                    set_position_size(saved_geometry.x, saved_geometry.y,
                                      saved_geometry.width,
                                      saved_geometry.height);
                else
                    set_position_size(output_box.x, output_box.y,
                                      output_box.width / 2,
                                      output_box.height / 2);
            }
        } else {
            // handles edge case where toplevel starts fullscreened
            if (saved_geometry.width && saved_geometry.height)
                // set back to saved geometry
                set_position_size(saved_geometry.x, saved_geometry.y,
                                  saved_geometry.width, saved_geometry.height);
            else
                // use half of output geometry
                set_position_size(output_box.x, output_box.y,
                                  output_box.width / 2, output_box.height / 2);
        }
    }
}

// set the toplevel to be maximized
void Toplevel::set_maximized(const bool maximized) {
    // HACK: do not maximize if not initialized
    if (xdg_toplevel && !xdg_toplevel->base->initialized)
        return;

    // unfullscreen if fullscreened
    if (fullscreen())
        set_fullscreen(false);

    // get output from toplevel's current workspace, fallback to
    // focused output
    const Output *output = nullptr;
    if (Workspace *workspace = server->get_workspace(this))
        output = workspace->output;
    else
        output = server->focused_output();

    // get usable output area
    wlr_box usable_area = output->usable_area;
    wlr_box output_box = output->layout_geometry;

    // set toplevel window mode to maximized
#ifdef XWAYLAND
    if (xdg_toplevel)
#endif
        wlr_xdg_toplevel_set_maximized(xdg_toplevel, maximized);
#ifdef XWAYLAND
    else {
        wlr_xwayland_surface_set_maximized(xwayland_surface, maximized,
                                           maximized);
        xwayland_maximized = maximized;
    }
#endif

    if (maximized) {
        // save current geometry
        save_geometry();

        // raise toplevel to top of scene tree
        wlr_scene_node_raise_to_top(&scene_tree->node);

        // set to top left of output, width and height the size of
        // output
        set_position_size(usable_area.x + output_box.x,
                          usable_area.y + output_box.y, usable_area.width,
                          usable_area.height);
    } else {
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
}

// create foreign toplevel
void Toplevel::create_foreign() {
    foreign_handle = wlr_foreign_toplevel_handle_v1_create(
        server->wlr_foreign_toplevel_manager);
    foreign_handle->data = this;

    // foreign toplevel activate
    foreign_activate.notify = [](wl_listener *listener, void *data) {
        Toplevel *toplevel =
            wl_container_of(listener, toplevel, foreign_activate);
        const auto *event =
            static_cast<wlr_foreign_toplevel_handle_v1_activated_event *>(data);
        if (event->seat == toplevel->server->seat->wlr_seat)
            toplevel->focus();
    };
    wl_signal_add(&foreign_handle->events.request_activate, &foreign_activate);

    // foreign_toplevel close
    foreign_close.notify = [](wl_listener *listener,
                              [[maybe_unused]] void *data) {
        Toplevel *toplevel = wl_container_of(listener, toplevel, foreign_close);
        toplevel->close();
    };
    wl_signal_add(&foreign_handle->events.request_close, &foreign_close);
}

// create ext foreign toplevel
void Toplevel::create_ext_foreign() {
    const std::string title = std::string(get_title());
    const std::string app_id = std::string(get_app_id());
    wlr_ext_foreign_toplevel_handle_v1_state state{
        title.c_str(),
        app_id.c_str(),
    };
    ext_foreign_handle = wlr_ext_foreign_toplevel_handle_v1_create(
        server->wlr_foreign_toplevel_list, &state);
    ext_foreign_handle->data = this;

    // ext foreign toplevel destroy
    ext_foreign_destroy.notify = [](wl_listener *listener,
                                    [[maybe_unused]] void *data) {
        Toplevel *toplevel =
            wl_container_of(listener, toplevel, ext_foreign_destroy);
        wlr_ext_foreign_toplevel_handle_v1_destroy(
            toplevel->ext_foreign_handle);
    };
    wl_signal_add(&ext_foreign_handle->events.destroy, &ext_foreign_destroy);
}

// update ext foreign toplevel
void Toplevel::update_ext_foreign() const {
    const std::string title = std::string(get_title());
    const std::string app_id = std::string(get_app_id());
    wlr_ext_foreign_toplevel_handle_v1_state state{
        title.c_str(),
        app_id.c_str(),
    };
    wlr_ext_foreign_toplevel_handle_v1_update_state(ext_foreign_handle, &state);
}

// update activation token with new one
void Toplevel::set_token(ActivationToken *token) {
    if (!token)
        return;

    if (activation_token) {
        activation_token->owning_toplevel = nullptr;
        delete activation_token;
        activation_token = nullptr;
    }

    token->consume();
    activation_token = token;
    token->owning_toplevel = this;
}

void Toplevel::update_pid() {
#ifdef XWAYLAND
    if (xwayland_surface) {
        pid = xwayland_surface->pid;
    } else {
#endif
        struct wl_client *client =
            wl_resource_get_client(xdg_toplevel->base->resource);
        wl_client_get_credentials(client, &pid, nullptr, nullptr);
#ifdef XWAYLAND
    }
#endif
}

// toggle maximized
void Toplevel::toggle_maximized() { set_maximized(!maximized()); }

// toggle fullscreen
void Toplevel::toggle_fullscreen() { set_fullscreen(!fullscreen()); }

// save the current geometry of the toplevel to saved_geometry
void Toplevel::save_geometry() {
    // save from geometry variable to get logical size (without
    // decoration offset)
    saved_geometry.x = geometry.x;
    saved_geometry.y = geometry.y;

    // only save width and height if they are set
    if (geometry.width && geometry.height) {
        saved_geometry.width = geometry.width;
        saved_geometry.height = geometry.height;
    }
}

// get the title of the toplevel
std::string_view Toplevel::get_title() const {
#ifdef XWAYLAND
    if (xwayland_surface)
        return xwayland_surface->title ? xwayland_surface->title : "";
#endif
    if (xdg_toplevel && xdg_toplevel->title)
        return xdg_toplevel->title;
    return "";
}

// get the app id of the toplevel
std::string_view Toplevel::get_app_id() const {
#ifdef XWAYLAND
    if (xwayland_surface)
        return xwayland_surface->class_ ? xwayland_surface->class_ : "";
#endif
    if (xdg_toplevel && xdg_toplevel->app_id)
        return xdg_toplevel->app_id;
    return "";
}

// update the title of the toplevel
void Toplevel::update_title() {
    const std::string title = std::string(get_title());

    if (foreign_handle && !title.empty())
        wlr_foreign_toplevel_handle_v1_set_title(foreign_handle, title.c_str());

    if (ext_foreign_handle)
        update_ext_foreign();
}

// update the app id of the toplevel
void Toplevel::update_app_id() {
    const std::string id = std::string(get_app_id());

    if (foreign_handle && !id.empty())
        wlr_foreign_toplevel_handle_v1_set_app_id(foreign_handle, id.c_str());

    if (ext_foreign_handle)
        update_ext_foreign();
}

// tell the toplevel to close
void Toplevel::close() {
#ifdef XWAYLAND
    if (xdg_toplevel)
#endif
        wlr_xdg_toplevel_send_close(xdg_toplevel);
#ifdef XWAYLAND
    else if (xwayland_surface)
        wlr_xwayland_surface_close(xwayland_surface);
#endif
}

// check if toplevel has size constraints (min or max size set)
bool Toplevel::has_size_constraints() const {
    if (xdg_toplevel) {
        bool has_min = xdg_toplevel->current.min_width > 0 ||
                       xdg_toplevel->current.min_height > 0;
        bool has_max = xdg_toplevel->current.max_width > 0 ||
                       xdg_toplevel->current.max_height > 0;
        return has_min || has_max;
    }
    return false;
}

// determine if this toplevel should be floating based on config and constraints
bool Toplevel::should_be_floating() const {
    if (!xdg_toplevel)
        return false;

    const auto &tiling_config = server->config->tiling;

    bool has_min = xdg_toplevel->current.min_width > 0 ||
                   xdg_toplevel->current.min_height > 0;
    bool has_max = xdg_toplevel->current.max_width > 0 ||
                   xdg_toplevel->current.max_height > 0;

    if (tiling_config.float_on_both && has_min && has_max)
        return true;
    if (tiling_config.float_on_min_size && has_min)
        return true;
    if (tiling_config.float_on_max_size && has_max)
        return true;

    return false;
}
