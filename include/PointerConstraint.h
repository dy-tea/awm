#pragma once

#include "wlr.h"

struct PointerConstraint {
    struct Cursor *cursor;
    wlr_pointer_constraint_v1 *constraint;

    wl_listener set_region;
    wl_listener destroy;

    PointerConstraint(wlr_pointer_constraint_v1 *constraint, Cursor *cursor);
    ~PointerConstraint();
};
