#include "wlr.h"

struct Output {
    wlr_output* output;
    wl_listener frame;
    wl_listener request_state;
    wl_listener destroy;
};
