#include "Transaction.h"
#include "Server.h"
#include "Toplevel.h"
#include "Workspace.h"

constexpr int TRANSACTION_TIMEOUT_MS = 300; // constexpr so fancy
Transaction::Transaction(Server *server) : server(server) {
    surface_commit.notify = nullptr;
}

Transaction::~Transaction() { cleanup(); }

void Transaction::add_change(Toplevel *toplevel, const wlr_box &geometry) {
    if (!toplevel)
        return;

    // check for already pending change
    for (auto &pending : pending_changes) {
        if (pending.toplevel == toplevel) {
            pending.geometry = geometry;
            return;
        }
    }

    // add new pending change
    PendingGeometry pending;
    pending.toplevel = toplevel;
    pending.geometry = geometry;
    pending.serial = 0;
    pending.committed = false;

    pending_changes.push_back(pending);
}

void Transaction::add_change(Toplevel *toplevel, int x, int y, int width,
                             int height) {
    add_change(toplevel, wlr_box{x, y, width, height});
}

void Transaction::commit() {
    if (committed)
        return;

    committed = true;

    if (pending_changes.empty())
        return;

    // send configures
    for (auto &pending : pending_changes) {
        Toplevel *toplevel = pending.toplevel;
        const wlr_box &geo = pending.geometry;

        // store pending geometry
        toplevel->pending_transaction_geometry = geo;
        toplevel->in_transaction = true;

        // update decoration before toplevel
        if (toplevel->decoration)
            toplevel->decoration->update_titlebar(geo.width);

        int width = geo.width;
        int height = geo.height;

        // apply offset if needed
        if (toplevel->decoration &&
            toplevel->decoration_mode ==
                WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE &&
            toplevel->decoration->visible) {
            int deco_height = 30;
            height -= deco_height;
        }

        if (width <= 0 || height <= 0) {
            waiting_for_commit.erase(toplevel);
            continue;
        }

#ifdef XWAYLAND
        if (toplevel->xdg_toplevel) {
#endif
            pending.serial = wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel,
                                                       width, height);
            wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);
#ifdef XWAYLAND
        } else if (toplevel->xwayland_surface) {
            wlr_xwayland_surface_configure(toplevel->xwayland_surface, geo.x,
                                           geo.y, width, height);
            pending.serial = 0;
        }
#endif

        waiting_for_commit.insert(toplevel);
    }

    setup_timeout();
}

void Transaction::apply() {
    if (pending_changes.empty()) {
        cleanup();
        return;
    }

    // check if has valid toplevels
    bool has_valid_toplevels = false;
    for (const auto &pending : pending_changes) {
        if (pending.toplevel && pending.toplevel->scene_tree) {
            has_valid_toplevels = true;
            break;
        }
    }

    if (!has_valid_toplevels) {
        cleanup();
        return;
    }

    // apply geometry changes
    for (const auto &pending : pending_changes) {
        Toplevel *toplevel = pending.toplevel;
        const wlr_box &geo = pending.geometry;
        if (!toplevel)
            continue;
        if (!toplevel->scene_tree)
            continue;
        if (!toplevel->server)
            continue;

        Workspace *workspace = toplevel->server->get_workspace(toplevel);
        if (!workspace)
            continue;

        bool surface_mapped = false;

#ifdef XWAYLAND
        wlr_xdg_toplevel *xdg = toplevel->xdg_toplevel;
        wlr_xwayland_surface *xwayland = toplevel->xwayland_surface;

        if (xdg) {
            wlr_xdg_surface *base = xdg->base;
            if (base) {
                wlr_surface *surface = base->surface;
                if (surface)
                    surface_mapped = surface->mapped;
            }
        } else if (xwayland) {
            wlr_surface *surface = xwayland->surface;
            if (surface)
                surface_mapped = surface->mapped;
        }
#else
        wlr_xdg_toplevel *xdg = toplevel->xdg_toplevel;
        if (xdg) {
            wlr_xdg_surface *base = xdg->base;
            if (base) {
                wlr_surface *surface = base->surface;
                if (surface)
                    surface_mapped = surface->mapped;
            }
        }
#endif

        if (!surface_mapped) {
            toplevel->in_transaction = false;
            continue;
        }

        // re-enable node
        if (toplevel->scene_hidden_for_autotile) {
            wlr_scene_node_set_enabled(&toplevel->scene_tree->node, true);
            toplevel->scene_hidden_for_autotile = false;
        }

        toplevel->geometry = geo;
        toplevel->in_transaction = false;

        int x = geo.x;
        int y = geo.y;

        // apply decoration offset if decoration is visible
        if (toplevel->decoration &&
            toplevel->decoration_mode ==
                WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE &&
            toplevel->decoration->visible) {
            int deco_height = 30;
            y += deco_height;
        }

#ifdef XWAYLAND
        if (toplevel->xdg_toplevel) {
#endif
            if (toplevel->scene_tree)
                wlr_scene_node_set_position(&toplevel->scene_tree->node, x, y);
#ifdef XWAYLAND
        } else if (toplevel->scene_surface && toplevel->scene_surface->buffer) {
            wlr_scene_node_set_position(&toplevel->scene_surface->buffer->node,
                                        x, y);
        }
#endif
    }

    // notify clients
    if (server->ipc)
        server->ipc->notify_clients({IPC_TOPLEVEL_LIST, IPC_WORKSPACE_LIST});

    cleanup();
}

bool Transaction::contains(Toplevel *toplevel) const {
    for (const auto &pending : pending_changes) {
        if (pending.toplevel == toplevel)
            return true;
    }
    return false;
}

void Transaction::handle_commit(Toplevel *toplevel) {
    if (!contains(toplevel))
        return;

    // mark toplevel as committed
    for (auto &pending : pending_changes) {
        if (pending.toplevel == toplevel) {
            pending.committed = true;
            break;
        }
    }

    // remove from waiting set
    waiting_for_commit.erase(toplevel);

    // apply transaction
    if (waiting_for_commit.empty()) {
        if (server->transaction_manager->current() == this) {
            server->transaction_manager->active_transaction = nullptr;
        }
        apply();
        delete this;
    }
}

void Transaction::remove_toplevel(Toplevel *toplevel) {
    if (!toplevel)
        return;

    // remove from waiting set
    waiting_for_commit.erase(toplevel);

    // mark as removed in pending changes
    for (auto &pending : pending_changes)
        if (pending.toplevel == toplevel)
            pending.toplevel = nullptr;

    // apply transaction
    if (committed && waiting_for_commit.empty() && !pending_changes.empty()) {
        if (server->transaction_manager->current() == this) {
            server->transaction_manager->active_transaction = nullptr;
        }
        apply();
        delete this;
    }
}

void Transaction::setup_timeout() {
    if (timeout_timer)
        wl_event_source_remove(timeout_timer);

    timeout_timer = wl_event_loop_add_timer(
        wl_display_get_event_loop(server->display), on_timeout, this);

    wl_event_source_timer_update(timeout_timer, TRANSACTION_TIMEOUT_MS);
}

void Transaction::cleanup() {
    if (timeout_timer) {
        wl_event_source_remove(timeout_timer);
        timeout_timer = nullptr;
    }

    waiting_for_commit.clear();

    // clear transaction state
    for (const auto &pending : pending_changes)
        if (pending.toplevel)
            pending.toplevel->in_transaction = false;
}

int Transaction::on_timeout(void *data) {
    Transaction *txn = static_cast<Transaction *>(data);
    if (txn->server->transaction_manager->current() == txn)
        txn->server->transaction_manager->active_transaction = nullptr;

    txn->apply();
    delete txn;
    return 0;
}

/* TransactionManager */

TransactionManager::TransactionManager(Server *server) : server(server) {}

TransactionManager::~TransactionManager() {
    if (active_transaction) {
        active_transaction->apply();
        delete active_transaction;
    }
}

Transaction *TransactionManager::begin() {
    if (active_transaction)
        commit();

    active_transaction = new Transaction(server);
    return active_transaction;
}

void TransactionManager::commit() {
    if (!active_transaction)
        return;

    Transaction *txn = active_transaction;

    txn->commit();

    if (txn->pending_changes.empty()) {
        active_transaction = nullptr;
        delete txn;
    }
}

void TransactionManager::remove_toplevel(Toplevel *toplevel) {
    if (active_transaction)
        active_transaction->remove_toplevel(toplevel);
}

// idfk
namespace TransactionHelper {
template <typename Func> void with_transaction(Server *server, Func &&func) {
    Transaction *txn = server->transaction_manager->begin();
    func(txn);
    server->transaction_manager->commit();
}
} // namespace TransactionHelper
