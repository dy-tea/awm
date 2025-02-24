#include "Server.h"

PointerConstraint::PointerConstraint(wlr_pointer_constraint_v1 *constraint)
    : constraint(constraint) {

    // destroy
    destroy.notify = [](struct wl_listener *listener, void *data) {
        PointerConstraint *constraint = wl_container_of(listener, constraint, destroy);
        delete constraint;
    };
    wl_signal_add(&constraint->events.destroy, &destroy);
}

PointerConstraint::~PointerConstraint() {
    wl_list_remove(&destroy.link);
}
