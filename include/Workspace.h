#pragma once

#include "wlr.h"
#include <vector>

struct Workspace {
    wl_list link;
    uint32_t num;
    struct Output *output;
    wl_list toplevels;
    struct Toplevel *active_toplevel{nullptr};

    Workspace(Output *output, uint32_t num);
    ~Workspace() = default;

    void add_toplevel(Toplevel *toplevel, bool focus);
    void close(Toplevel *toplevel);
    void close_active();
    bool contains(const Toplevel *toplevel) const;
    bool move_to(Toplevel *toplevel, Workspace *workspace);
    void swap(Toplevel *a, Toplevel *b) const;
    void swap(Toplevel *other) const;
    Toplevel *in_direction(wlr_direction direction) const;
    void set_hidden(bool hidden) const;
    void set_half_in_direction(Toplevel *toplevel, wlr_direction direction);
    void focus();
    void focus_toplevel(Toplevel *toplevel);
    void focus_next();
    void focus_prev();
    void tile(std::vector<Toplevel *> sans_toplevels);
    void tile();
    void tile_sans_active();
    std::vector<Toplevel *> pinned();
};
