#pragma once

#include "wlr.h"
#include <vector>
#include <unordered_set>

struct PendingGeometry {
    struct Toplevel *toplevel;
    wlr_box geometry;
    uint32_t serial;
    bool committed{false};
};

struct Transaction {
    struct Server *server;
    std::vector<PendingGeometry> pending_changes;
    std::unordered_set<struct Toplevel *> waiting_for_commit;
    wl_event_source *timeout_timer{nullptr};
    bool committed{false};

    wl_listener surface_commit;

    explicit Transaction(struct Server *server);
    ~Transaction();

    // Add a geometry change to this transaction
    void add_change(Toplevel *toplevel, const wlr_box &geometry);
    void add_change(Toplevel *toplevel, int x, int y, int width, int height);

    // Commit the transaction (send configure events and wait for responses)
    void commit();

    // Apply all changes immediately (called after all surfaces commit or timeout)
    void apply();

    // Check if a toplevel is part of this transaction
    bool contains(Toplevel *toplevel) const;

    // Handle surface commit from a toplevel
    void handle_commit(Toplevel *toplevel);
    
    // Remove a toplevel from this transaction (e.g., when it unmaps)
    void remove_toplevel(Toplevel *toplevel);

private:
    void setup_timeout();
    void cleanup();
    static int on_timeout(void *data);
};

// Transaction manager handles the active transaction
struct TransactionManager {
    Server *server;
    Transaction *active_transaction{nullptr};

    explicit TransactionManager(Server *server);
    ~TransactionManager();

    // Start a new transaction (commits any existing one first)
    Transaction *begin();

    // Get the current active transaction, or nullptr if none
    Transaction *current() const { return active_transaction; }

    // Check if a transaction is currently active
    bool is_active() const { return active_transaction != nullptr; }

    // Commit and apply the current transaction
    void commit();
    
    // Remove a toplevel from any active transaction
    void remove_toplevel(Toplevel *toplevel);
};
