#pragma once

#include "wlr.h"
#include <memory>
#include <vector>

struct BSPTree;

struct Workspace {
    wl_list link;
    uint32_t num;
    struct Output *output;
    wl_list toplevels;
    struct Toplevel *active_toplevel{nullptr};
    bool auto_tile{false};
    std::unique_ptr<BSPTree> bsp_tree{nullptr};
    wl_event_source *pending_layout_idle{nullptr};

    Workspace(Output *output, uint32_t num);
    ~Workspace();

    void add_toplevel(Toplevel *toplevel, bool focus);
    Toplevel *get_topmost_toplevel(Toplevel *sans = nullptr) const;
    void close(Toplevel *toplevel);
    void close_active();
    bool contains(const Toplevel *toplevel) const;
    bool move_to(Toplevel *toplevel, Workspace *workspace);
    void swap(Toplevel *a, Toplevel *b);
    void swap(Toplevel *other);
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
    void toggle_auto_tile();
    void adjust_neighbors_on_resize(Toplevel *resized, const wlr_box &old_geo);
    std::vector<Toplevel *> fullscreen_toplevels();
    std::vector<Toplevel *> pinned();
};
