#include "wlr.h"

struct Popup {
    wl_list link;
    wlr_xdg_popup *popup;
    wl_listener commit;
    wl_listener destroy;
};
