#include "wlr.h"

struct Popup {
   wlr_xdg_popup *popup;
   wl_listener commit;
   wl_listener destroy;
};
