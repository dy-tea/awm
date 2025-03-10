#include "Server.h"

PointerConstraint::PointerConstraint(wlr_pointer_constraint_v1 *constraint,
                                     Cursor *cursor)
    : cursor(cursor), constraint(constraint) {

    // set_region
    set_region.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        PointerConstraint *constraint =
            wl_container_of(listener, constraint, set_region);
        constraint->cursor->requires_warp = true;
    };
    wl_signal_add(&constraint->events.set_region, &set_region);

    // destroy
    destroy.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        PointerConstraint *constraint =
            wl_container_of(listener, constraint, destroy);
        delete constraint;
    };
    wl_signal_add(&constraint->events.destroy, &destroy);
}

PointerConstraint::~PointerConstraint() {
    wl_list_remove(&destroy.link);
    wl_list_remove(&set_region.link);

    if (cursor->active_constraint == constraint) {
        cursor->warp_to_constraint_hint();

        if (cursor->active_constraint->link.next)
            wl_list_remove(&cursor->constraint_commit.link);

        wl_list_init(&cursor->constraint_commit.link);
        cursor->active_constraint = nullptr;
    }
}
