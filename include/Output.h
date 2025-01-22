#include "wlr.h"

struct Output {
    wl_list link;
    wlr_output* output;
    wl_listener frame;
    wl_listener request_state;
    wl_listener destroy;
};
