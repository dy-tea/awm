#include "wlr.h"

struct Popup {
    Server *server;
    wl_list link;
    wlr_xdg_popup *xdg_popup;
    wl_listener commit;
    wl_listener destroy;

    Popup(wlr_xdg_popup *xdg_popup, Server *server);
    ~Popup();
};
