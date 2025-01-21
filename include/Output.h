#include "wlr.h"
#include <vector>

struct Output {
    std::vector<Output*> *link;
    wlr_output* output;
    wl_listener frame;
    wl_listener request_state;
    wl_listener destroy;
};
