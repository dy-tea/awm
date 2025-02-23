#include "wlr.h"

struct Workspace {
    wl_list link;
    uint32_t num;
    Output *output;
    wl_list toplevels;
    Toplevel *active_toplevel{nullptr};

    Workspace(Output *output, uint32_t num);
    ~Workspace() = default;

    void add_toplevel(Toplevel *toplevel, bool focus);
    void close(const Toplevel *toplevel);
    void close_active();
    bool contains(const Toplevel *toplevel) const;
    bool move_to(Toplevel *toplevel, Workspace *workspace);
    void swap(Toplevel *other) const;
    Toplevel *in_direction(wlr_direction direction) const;
    void set_hidden(bool hidden);
    void focus();
    void focus_toplevel(Toplevel *toplevel);
    void focus_next();
    void focus_prev();
    void tile();
};
