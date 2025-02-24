#include "wlr.h"

struct PointerConstraint {
    wlr_pointer_constraint_v1 *constraint;
    wl_listener destroy;

    PointerConstraint(wlr_pointer_constraint_v1 *constraint);
    ~PointerConstraint();
};
