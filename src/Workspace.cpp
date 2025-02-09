#include "Server.h"
#include <climits>

Workspace::Workspace(struct Output *output, uint32_t num) {
    this->output = output;
    this->num = num;
    wl_list_init(&toplevels);
}

Workspace::~Workspace() {}

// add a toplevel to the workspace
void Workspace::add_toplevel(struct Toplevel *toplevel) {
    wl_list_insert(&toplevels, &toplevel->link);
    active_toplevel = toplevel;
    toplevel->focus();
}

// close the active toplevel
void Workspace::close_active() {
    if (active_toplevel) {
        if (wl_list_length(&toplevels) > 1) {
            // focus the next toplevel
            Toplevel *new_active =
                wl_container_of(toplevels.prev, new_active, link);
            new_active->focus();
        }

        // tell the toplevel to close
        active_toplevel->close();
    }
}

// returns true if the workspace contains the passed toplevel
bool Workspace::contains(struct Toplevel *toplevel) {
    if (wl_list_empty(&toplevels))
        return false;

    Toplevel *current, *tmp;
    wl_list_for_each_safe(current, tmp, &toplevels,
                          link) if (current == toplevel) return true;

    return false;
}

// move a toplevel to another workspace, returns true on success
// false on no movement or failure
bool Workspace::move_to(struct Toplevel *toplevel,
                        struct Workspace *workspace) {
    // no movement
    if (workspace == this)
        return false;

    // active toplevel needs to be updated
    if (toplevel == active_toplevel) {
        if (wl_list_length(&toplevels) > 1) {
            // focus the next toplevel
            Toplevel *new_active =
                wl_container_of(toplevels.prev, new_active, link);
            new_active->focus();
        } else
            // no more active toplevel
            active_toplevel = nullptr;
    }

    // ensure toplevel is part of workspace
    if (contains(toplevel)) {
        // move to other workspace
        toplevel->set_hidden(true);
        wl_list_remove(&toplevel->link);
        workspace->add_toplevel(toplevel);

        return true;
    }

    return false;
}

// set the workspace's visibility
void Workspace::set_hidden(bool hidden) {
    Toplevel *toplevel, *tmp;
    wl_list_for_each_safe(toplevel, tmp, &toplevels, link)
        toplevel->set_hidden(hidden);
}

// swap the active toplevel geometry with other toplevel geometry
void Workspace::swap(struct Toplevel *other) {
    struct wlr_fbox active = active_toplevel->get_geometry();
    struct wlr_fbox swapped = other->get_geometry();

    active_toplevel->set_position_size(swapped);
    other->set_position_size(active);
}

// get the toplevel relative to the active one in the specified direction
// returns nullptr if no toplevel matches query
struct Toplevel *Workspace::in_direction(enum wlr_direction direction) {
    // no other toplevel to focus
    if (wl_list_length(&toplevels) < 2)
        return nullptr;

    // get the geometry of the active toplevel
    wlr_fbox active_geometry = active_toplevel->get_geometry();

    // define a target
    Toplevel *curr, *tmp;
    double min_distance = std::numeric_limits<double>::max();
    double other = std::numeric_limits<double>::max();
    Toplevel *target = nullptr;

    // find the smallest positive distance, absolute axis
    auto validate = [&](double distance, double axis) {
        // note the greater or equals is needed for tiled windows since they
        // are placed perfectly on the same axis
        if (distance > 0 && distance <= min_distance && axis < other) {
            target = curr;
            min_distance = distance;
            other = axis;
        }
    };

    // get the non-negative displacement for each direction,
    // - find the smallest distance
    // - find the smallest absolute difference in the other axis
    switch (direction) {
    case WLR_DIRECTION_UP:
        wl_list_for_each_safe(curr, tmp, &toplevels, link)
            validate(active_geometry.y - curr->get_geometry().y,
                     std::abs(active_geometry.x - curr->get_geometry().x));
        break;
    case WLR_DIRECTION_DOWN:
        wl_list_for_each_safe(curr, tmp, &toplevels, link)
            validate(curr->get_geometry().y - active_geometry.y,
                     std::abs(active_geometry.x - curr->get_geometry().x));
        break;
    case WLR_DIRECTION_LEFT:
        wl_list_for_each_safe(curr, tmp, &toplevels, link)
            validate(active_geometry.x - curr->get_geometry().x,
                     std::abs(active_geometry.y - curr->get_geometry().y));
        break;
    case WLR_DIRECTION_RIGHT:
        wl_list_for_each_safe(curr, tmp, &toplevels, link)
            validate(curr->get_geometry().x - active_geometry.x,
                     std::abs(active_geometry.y - curr->get_geometry().y));
        break;
    default:
        break;
    }

    // this will be null if a toplevel is not found in the specified direction
    return target;
}

// focus the workspace
void Workspace::focus() {
    set_hidden(false);

    // ensure there is a toplevel to focus
    if (!wl_list_empty(&toplevels)) {
        if (active_toplevel)
            // focus the active toplevel if available
            active_toplevel->focus();
        else {
            // focus the first toplevel and set is as active
            Toplevel *toplevel =
                wl_container_of(toplevels.prev, toplevel, link);
            toplevel->focus();
        }
    }
}

// focus the passed toplevel
void Workspace::focus_toplevel(struct Toplevel *toplevel) {
    if (!contains(toplevel))
        return;

    // set toplevel to active
    active_toplevel = toplevel;

    // call keyboard focus
    toplevel->focus();
}

// focus the toplevel following the active one, looping around to the start
void Workspace::focus_next() {
    // no movement
    if (wl_list_length(&toplevels) < 2)
        return;

    // focus next, wrapping around if at end
    struct wl_list *next = active_toplevel->link.next;
    if (next == &toplevels)
        next = toplevels.next;

    Toplevel *next_toplevel = wl_container_of(next, next_toplevel, link);
    focus_toplevel(next_toplevel);
}

// focus the toplevel preceding the active one, looping around to the end
void Workspace::focus_prev() {
    // no movement
    if (wl_list_length(&toplevels) < 2)
        return;

    // focus prev, wrapping around if at start
    struct wl_list *prev = active_toplevel->link.prev;
    if (prev == &toplevels)
        prev = toplevels.prev;

    Toplevel *prev_toplevel = wl_container_of(prev, prev_toplevel, link);
    focus_toplevel(prev_toplevel);
}

// auto-tile the toplevels of a workspace, not currently reversible or
// any kind of special state
void Workspace::tile() {
    // no toplevels to tile
    if (wl_list_empty(&toplevels))
        return;

    // get the output geometry
    struct wlr_box box;
    wlr_output_layout_get_box(output->server->output_layout, output->wlr_output,
                              &box);

    int toplevel_count = wl_list_length(&toplevels);

    // calculate rows and cols from toplevel count
    int cols = std::round(std::sqrt(toplevel_count));
    int rows = (toplevel_count + cols - 1) / cols;

    // width and height is just the fraction of the output binds
    int width = box.width / cols;
    int height = box.height / rows;

    // loop through each toplevel
    int i = 0;
    Toplevel *toplevel, *tmp;
    wl_list_for_each_safe(toplevel, tmp, &toplevels, link) {
        // calculate toplevel geometry
        int row = i / cols;
        int col = i % cols;
        int x = box.x + (col * width);
        int y = box.y + (row * height);

        // set toplevel geometry
        toplevel->set_position_size(x, y, width, height);
        ++i;
    }
}
