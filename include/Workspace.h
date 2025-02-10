#include "wlr.h"

struct Workspace {
    struct wl_list link;
    uint32_t num;
    struct Output *output;
    struct wl_list toplevels;
    struct Toplevel *active_toplevel{nullptr};

    Workspace(struct Output *output, uint32_t num);
    ~Workspace();

    void add_toplevel(struct Toplevel *toplevel);
    void close_active();
    bool contains(struct Toplevel *toplevel);
    bool move_to(struct Toplevel *toplevel, struct Workspace *workspace);
    void swap(struct Toplevel *other);
    struct Toplevel *in_direction(enum wlr_direction direction);
    void set_hidden(bool hidden);
    void focus();
    void focus_toplevel(struct Toplevel *toplevel);
    void focus_next();
    void focus_prev();
    void tile();
};
