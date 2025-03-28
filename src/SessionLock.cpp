#include "Server.h"
#include "wlr.h"

SessionLock::SessionLock(Server *server, wlr_session_lock_v1 *session_lock)
    : server(server), session_lock(session_lock) {
    scene_tree = wlr_scene_tree_create(server->layers.lock);
    server->current_session_lock = session_lock;

    new_surface.notify = [](wl_listener *listener, void *data) {
        SessionLock *lock = wl_container_of(listener, lock, new_surface);
        Server *server = lock->server;
        auto *surface = static_cast<wlr_session_lock_surface_v1 *>(data);

        // set up scene tree for surface
        Output *output = server->get_output(surface->output);
        wlr_scene_tree *scene_tree = wlr_scene_subsurface_tree_create(
            lock->scene_tree, surface->surface);
        surface->surface->data = scene_tree;
        output->lock_surface = surface;

        // configure surface to output
        wlr_box box = output->layout_geometry;
        wlr_scene_node_set_position(&scene_tree->node, box.x, box.y);
        wlr_session_lock_surface_v1_configure(surface, box.width, box.height);

        // set up destructor for surface
        output->destroy_lock_surface.notify = [](wl_listener *listener,
                                                 [[maybe_unused]] void *data) {
            Output *output =
                wl_container_of(listener, output, destroy_lock_surface);
            Server *server = output->server;
            wlr_session_lock_surface_v1 *surface = output->lock_surface;

            // clear the surface and its destroy event
            output->lock_surface = nullptr;
            wl_list_remove(&output->destroy_lock_surface.link);

            // surface must be focused
            if (surface->surface !=
                server->seat->keyboard_state.focused_surface)
                return;

            if (server->locked && server->current_session_lock &&
                !wl_list_empty(&server->current_session_lock->surfaces)) {
                surface = wl_container_of(
                    server->current_session_lock->surfaces.next, surface, link);

                // give the surface keyboard focus
                wlr_keyboard *kb = wlr_seat_get_keyboard(server->seat);
                wlr_seat_keyboard_notify_enter(server->seat, surface->surface,
                                               kb->keycodes, kb->num_keycodes,
                                               &kb->modifiers);
            } else if (!server->locked)
                output->get_active()->focus();
            else
                wlr_seat_keyboard_notify_clear_focus(server->seat);
        };
        wl_signal_add(&surface->events.destroy, &output->destroy_lock_surface);

        // give keyboard focus to output
        if (server->focused_output() == output) {
            wlr_keyboard *kb = wlr_seat_get_keyboard(server->seat);
            wlr_seat_keyboard_notify_enter(server->seat, surface->surface,
                                           kb->keycodes, kb->num_keycodes,
                                           &kb->modifiers);
        }
    };
    wl_signal_add(&session_lock->events.new_surface, &new_surface);

    unlock.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        SessionLock *lock = wl_container_of(listener, lock, unlock);
        lock->destroy_unlock(true);
    };
    wl_signal_add(&session_lock->events.unlock, &unlock);

    destroy.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        SessionLock *lock = wl_container_of(listener, lock, destroy);
        lock->destroy_unlock(false);
    };
    wl_signal_add(&session_lock->events.destroy, &destroy);

    // lock the session
    wlr_session_lock_v1_send_locked(session_lock);
    server->locked = true;
}

SessionLock::~SessionLock() {
    wl_list_remove(&new_surface.link);
    wl_list_remove(&unlock.link);
    wl_list_remove(&destroy.link);

    wlr_scene_node_destroy(&scene_tree->node);
    server->current_session_lock = nullptr;
}

void SessionLock::destroy_unlock(const bool unlock) {
    // clear keyboard focus
    wlr_seat_keyboard_notify_clear_focus(server->seat);

    // unlock session if requested
    if (!(server->locked = !unlock)) {
        // disable lock background
        wlr_scene_node_set_enabled(&server->lock_background->node, false);

        // focus the active workspace
        if (Workspace *workspace = server->focused_output()->get_active())
            workspace->focus();
    }

    delete this;
}
