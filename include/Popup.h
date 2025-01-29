#include "wlr.h"

struct Popup {
    struct wl_list link;
    struct wlr_xdg_popup *xdg_popup;
    struct wl_listener commit;
    struct wl_listener destroy;

    Popup(struct wlr_xdg_popup *xdg_popup);
    ~Popup();
};
