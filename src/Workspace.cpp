#include "Server.h"

Workspace::Workspace(struct Output *output) {
    this->output = output;
    wl_list_init(&toplevels);
}

Workspace::~Workspace() {}

void Workspace::add_toplevel(struct Toplevel *toplevel) {
    wl_list_insert(&toplevels, &toplevel->link);
}

bool Workspace::contains(struct Toplevel *toplevel) {
    if (wl_list_empty(&toplevels))
        return false;

    Toplevel *current, *tmp;
    wl_list_for_each_safe(current, tmp, &toplevels,
                          link) if (current == toplevel) return true;

    return false;
}

bool Workspace::move_to(struct Toplevel *toplevel,
                        struct Workspace *workspace) {
    if (workspace == this)
        return false;

    if (contains(toplevel)) {
        wl_list_remove(&toplevel->link);
        workspace->add_toplevel(toplevel);
        return true;
    }

    return false;
}

struct Toplevel *Workspace::get_toplevel(uint32_t n) {
    Toplevel *toplevel, *tmp;
    int current = 0;
    wl_list_for_each_safe(toplevel, tmp, &toplevels, link) {
        if (current == n)
            return toplevel;
        else
            ++n;
    }

    return nullptr;
}

bool Workspace::move_to(uint32_t n, struct Workspace *workspace) {
    if (wl_list_empty(&toplevels))
        return false;

    Toplevel *toplevel = get_toplevel(n);

    if (toplevel == nullptr)
        return false;

    return move_to(toplevel, workspace);
}
