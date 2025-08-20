#include "Workspace.h"
#include "Config.h"
#include "IPC.h"
#include "Output.h"
#include "Seat.h"
#include "Server.h"
#include "Toplevel.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

Workspace::Workspace(Output *output, const uint32_t num)
    : num(num), output(output) {
    wl_list_init(&toplevels);
}

Workspace::~Workspace() {
    Toplevel *toplevel, *tmp;
    wl_list_for_each_safe(toplevel, tmp, &toplevels, link) delete toplevel;

    wl_list_remove(&link);
}

// add a toplevel to the workspace
void Workspace::add_toplevel(Toplevel *toplevel, const bool focus) {
    // ensure toplevel is not already in workspace
    if (contains(toplevel))
        return;

    // add to toplevels list
    wl_list_insert(&toplevels, &toplevel->link);

    // set active
    active_toplevel = toplevel;

    // focus
    if (focus)
        toplevel->focus();
}

// close a toplevel
void Workspace::close(Toplevel *toplevel) {
    if (!toplevel)
        return;

    // active toplevels need extra handling
    if (toplevel == active_toplevel) {
        if (wl_list_length(&toplevels) > 1)
            // focus the next toplevel
            focus_next();
        else {
            // no more active toplevel
            active_toplevel = nullptr;

            // clear keyboard focus
            wlr_seat_keyboard_notify_clear_focus(
                output->server->seat->wlr_seat);
        }
    }

    // send close
    toplevel->close();
}

// close the active toplevel
void Workspace::close_active() { close(active_toplevel); }

// returns true if the workspace contains the passed toplevel
bool Workspace::contains(const Toplevel *toplevel) const {
    // no toplevels
    if (wl_list_empty(&toplevels))
        return false;

    // check if toplevel is in workspace
    Toplevel *current, *tmp;
    wl_list_for_each_safe(current, tmp, &toplevels,
                          link) if (current == toplevel) return true;

    // toplevel not found
    return false;
}

// move a toplevel to another workspace, returns true on success
// false on no movement or failure
bool Workspace::move_to(Toplevel *toplevel, Workspace *workspace) {
    // no movement
    if (workspace == this || workspace->fullscreen_toplevel ||
        !contains(toplevel))
        return false;

    // only hide toplevel if moving to a workspace on the same output
    if (output == output->server->focused_output())
        toplevel->set_hidden(true);

    // update active_toplevel if necessary
    if (toplevel == active_toplevel) {
        if (wl_list_length(&toplevels) > 1)
            // focus the next toplevel
            focus_next();
        else
            // no more active toplevel
            active_toplevel = nullptr;
    }

    // move to other workspace
    wl_list_remove(&toplevel->link);
    workspace->add_toplevel(toplevel, true);

    // update fullscreen_toplevel if necessary
    if (fullscreen_toplevel == toplevel) {
        workspace->fullscreen_toplevel = toplevel;
        fullscreen_toplevel = nullptr;
    }

    // notify clients
    if (IPC *ipc = toplevel->server->ipc)
        ipc->notify_clients({IPC_TOPLEVEL_LIST, IPC_WORKSPACE_LIST});

    return true;
}

// set the workspace visibility
void Workspace::set_hidden(const bool hidden) const {
    Toplevel *toplevel, *tmp;
    wl_list_for_each_safe(toplevel, tmp, &toplevels, link)
        // set every toplevel to the value of hidden
        toplevel->set_hidden(hidden);
}

// swap the geometry of toplevel a and b
void Workspace::swap(Toplevel *a, Toplevel *b) const {
    if (!a || !b)
        return;

    // get the geometry of both toplevels
    const wlr_box geo_a = a->geometry;
    const wlr_box geo_b = b->geometry;

    // swap the geometry
    a->set_position_size(geo_b);
    b->set_position_size(geo_a);
}

// swap the active toplevel geometry with other toplevel geometry
void Workspace::swap(Toplevel *other) const { swap(active_toplevel, other); }

// get the toplevel relative to the active one in the specified direction
// returns nullptr if no toplevel matches query
Toplevel *Workspace::in_direction(const wlr_direction direction) const {
    // no other toplevel to focus or no active toplevel
    if (wl_list_length(&toplevels) < 2 || !active_toplevel)
        return nullptr;

    // get the geometry of the active toplevel
    const wlr_box &active_geometry = active_toplevel->geometry;

    // define a target
    Toplevel *curr, *tmp;
    int min_primary = std::numeric_limits<int>::max();
    int min_secondary = std::numeric_limits<int>::max();
    Toplevel *target = nullptr;

    // find the smallest positive distance, absolute axis
    auto process = [&](const int primary, const int secondary) {
        // note the greater or equals is needed for tiled windows since they
        // are placed perfectly on the same axis
        if (primary > 0 && primary <= min_primary &&
            secondary <= min_secondary) {
            target = curr;
            min_primary = primary;
            min_secondary = secondary;
        }
    };

    // get the non-negative displacement for each direction,
    // - find the smallest distance
    // - find the smallest absolute difference in the other axis
    switch (direction) {
    case WLR_DIRECTION_UP:
        wl_list_for_each_safe(curr, tmp, &toplevels, link)
            process(active_geometry.y - curr->geometry.y,
                    std::abs(active_geometry.x - curr->geometry.x));
        break;
    case WLR_DIRECTION_DOWN:
        wl_list_for_each_safe(curr, tmp, &toplevels, link)
            process(curr->geometry.y - active_geometry.y,
                    std::abs(active_geometry.x - curr->geometry.x));
        break;
    case WLR_DIRECTION_LEFT:
        wl_list_for_each_safe(curr, tmp, &toplevels, link)
            process(active_geometry.x - curr->geometry.x,
                    std::abs(active_geometry.y - curr->geometry.y));
        break;
    case WLR_DIRECTION_RIGHT:
        wl_list_for_each_safe(curr, tmp, &toplevels, link)
            process(curr->geometry.x - active_geometry.x,
                    std::abs(active_geometry.y - curr->geometry.y));
        break;
    default:
        break;
    }

    // this will be null if a toplevel is not found in the specified direction
    return target;
}

// set the toplevel to take up half the screen in the given direction
void Workspace::set_half_in_direction(Toplevel *toplevel,
                                      wlr_direction direction) {
    int x = output->layout_geometry.x + output->usable_area.x;
    int y = output->layout_geometry.y + output->usable_area.y;
    int width = output->usable_area.width;
    int height = output->usable_area.height;
    switch (direction) {
    case WLR_DIRECTION_UP:
        height /= 2;
        break;
    case WLR_DIRECTION_DOWN:
        height /= 2;
        y += height;
        break;
    case WLR_DIRECTION_LEFT:
        width /= 2;
        break;
    case WLR_DIRECTION_RIGHT:
        width /= 2;
        x += width;
        break;
    default:
        throw std::runtime_error("invalid direction");
        break;
    }
    toplevel->set_position_size(x, y, width, height);
}

// focus the workspace
void Workspace::focus() {
    set_hidden(false);

    // ensure there is a toplevel to focus
    if (wl_list_empty(&toplevels))
        active_toplevel = nullptr;
    else {
        // focus the active toplevel if available,
        // otherwise focus the first toplevel and set is as active
        Toplevel *toplevel =
            active_toplevel ? active_toplevel
                            : wl_container_of(toplevels.prev, toplevel, link);
        active_toplevel = toplevel;
        if (toplevel)
            toplevel->focus();
    }

    // notify clients
    if (IPC *ipc = output->server->ipc)
        ipc->notify_clients({IPC_OUTPUT_LIST, IPC_WORKSPACE_LIST});
}

// focus the passed toplevel
void Workspace::focus_toplevel(Toplevel *toplevel) {
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
    wl_list *next = active_toplevel->link.next;
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
    wl_list *prev = active_toplevel->link.prev;
    if (prev == &toplevels)
        prev = toplevels.prev;

    Toplevel *prev_toplevel = wl_container_of(prev, prev_toplevel, link);
    focus_toplevel(prev_toplevel);
}

// auto-tile the toplevels of a workspace, not currently reversible or
// any kind of special state
void Workspace::tile(std::vector<Toplevel *> sans_toplevels) {
    // no toplevels to tile
    if (wl_list_empty(&toplevels))
        return;

    // get the output's usable area
    wlr_box box = output->usable_area;

    int toplevel_count = wl_list_length(&toplevels);

    // do not tile fullscreen toplevels
    Toplevel *toplevel, *tmp;
    std::vector<Toplevel *> tiled;
    wl_list_for_each_safe(toplevel, tmp, &toplevels, link) {
        if (toplevel->fullscreen())
            --toplevel_count;
        else
            tiled.push_back(toplevel);
    }

    // remove sans_toplevels from tiled list
    for (Toplevel *toplevel : sans_toplevels) {
        std::vector<Toplevel *>::iterator it =
            std::find(tiled.begin(), tiled.end(), toplevel);
        if (it != tiled.end()) {
            tiled.erase(it);
            --toplevel_count;
        }
    }

    // ensure there is a toplevel to tile
    if (!toplevel_count)
        return;

    // tile according to tiling method
    switch (output->server->config->tiling.method) {
    case TILE_GRID: {
        // calculate rows and cols from toplevel count
        int rows = std::round(std::sqrt(toplevel_count));
        int cols = (toplevel_count + rows - 1) / rows;

        // width and height is just the fraction of the output binds
        int width = box.width / cols;
        int height = box.height / rows;

        // sort by row and column order
        std::sort(tiled.begin(), tiled.end(), [&](Toplevel *a, Toplevel *b) {
            int ar = a->geometry.y / height;
            int ac = a->geometry.x / width;
            int br = b->geometry.y / height;
            int bc = b->geometry.x / width;

            // if rows are equal compare columns
            if (ar == br)
                return ac < bc;

            // otherwise compare rows
            return ar < br;
        });

        // loop through each toplevel
        for (int i = 0; i < toplevel_count - 1; ++i) {
            // calculate toplevel geometry
            int row = i / cols;
            int col = i % cols;
            int x = output->layout_geometry.x + box.x + (col * width);
            int y = output->layout_geometry.y + box.y + (row * height);

            // set toplevel geometry
            tiled[i]->set_position_size(x, y, width, height);
        }

        // calculate last toplevel geometry
        int i = toplevel_count - 1;
        int row = i / cols;
        int col = i % cols;
        int x = output->layout_geometry.x + box.x + (col * width);
        int y = output->layout_geometry.y + box.y + (row * height);

        // stretch width to fill remaining space
        if (int area = cols * rows; area != toplevel_count)
            width *= (1 + area - toplevel_count);

        // set last toplevel geometry
        tiled[i]->set_position_size(x, y, width, height);

        break;
    }
    case TILE_MASTER:
        if (toplevel_count == 1) {
            // take up full screen
            tiled[0]->set_position_size(box.x, box.y, box.width, box.height);
        } else {
            // calculate slave toplevel geometry
            int width = box.width / 2;
            int count = toplevel_count - 1;
            int height = box.height / count;

            // find master toplevel index
            std::vector<Toplevel *>::iterator it = tiled.begin();
            for (std::vector<Toplevel *>::iterator i = it + 1; i < tiled.end();
                 ++i)
                if ((*i)->geometry.x + (*i)->geometry.y <
                    (*it)->geometry.x + (*it)->geometry.y)
                    it = i;

            // set master toplevel geometry
            (*it)->set_position_size(box.x, box.y, width, box.height);

            // remove from list
            tiled.erase(it);

            // sort by y
            std::sort(tiled.begin(), tiled.end(),
                      [&](Toplevel *a, Toplevel *b) {
                          if (a->geometry.y == b->geometry.y)
                              return true;
                          return a->geometry.y < b->geometry.y;
                      });

            // x position is always half of the output width
            int x = output->layout_geometry.x + box.x + width;
            int y = output->layout_geometry.y + box.y;

            // set slave toplevel geometry
            for (int i = 0; i != count; ++i)
                tiled[i]->set_position_size(x, y + i * height, width, height);
        }

        break;
    case TILE_DWINDLE: {
        // start with width and height being the full output area
        int width = box.width;
        int height = box.height;
        int x = output->layout_geometry.x + box.x;
        int y = output->layout_geometry.y + box.y;

        // sort by sum of x and y
        std::sort(tiled.begin(), tiled.end(), [&](Toplevel *a, Toplevel *b) {
            // with equal x values assume second toplevel is greater
            if (a->geometry.x == b->geometry.x)
                return true;

            // otherwise compare sums
            return a->geometry.x + a->geometry.y <
                   b->geometry.x + b->geometry.y;
        });

        // 1 toplevel means that it should take up the full screen
        if (tiled.size() != 1)
            width /= 2;

        for (int i = 0; i != toplevel_count; ++i) {
            // set toplevel geometry
            tiled[i]->set_position_size(x, y, width, height);

            if (i % 2) {
                // do not change size for last toplevel
                if (i != toplevel_count - 2)
                    width /= 2;
                // add height to y
                y += height;
            } else {
                // do not change size for last toplevel
                if (i != toplevel_count - 2)
                    height /= 2;
                // add width to x
                x += width;
            }
        }

        break;
    }
    default:
        break;
    }
}

// no argument tile means tile all toplevels
void Workspace::tile() { tile({}); }

// tile without active toplevel
void Workspace::tile_sans_active() {
    if (active_toplevel)
        tile({active_toplevel});
}

// get pinned toplevels
std::vector<Toplevel *> Workspace::pinned() {
    std::vector<Toplevel *> pinned;

    Toplevel *toplevel, *tmp;
    wl_list_for_each_safe(toplevel, tmp, &toplevels, link) if (toplevel->pinned)
        pinned.push_back(toplevel);

    return pinned;
}
