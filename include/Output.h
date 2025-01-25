#include "wlr.h"

struct Output {
	struct wl_list link;
	struct Server *server;
	struct wlr_output *wlr_output;
	struct wl_listener frame;
	struct wl_listener request_state;
	struct wl_listener destroy;

	Output(struct Server *server, struct wlr_output* wlr_output);
};
