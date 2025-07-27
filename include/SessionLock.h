#pragma once

#include "wlr.h"

struct SessionLock {
    struct Server *server;
    wlr_scene_tree *scene_tree;
    wlr_session_lock_v1 *session_lock;

    wl_listener new_surface;
    wl_listener unlock;
    wl_listener destroy;

    SessionLock(Server *server, wlr_session_lock_v1 *session_lock);
    ~SessionLock();

    void destroy_unlock(const bool unlock);
};
