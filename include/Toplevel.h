#include "wlr.h"
#include "CursorMode.h"

struct Toplevel {
	struct wl_list link;
	struct Server *server;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener commit;
	struct wl_listener destroy;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;

	Toplevel(struct Server *server, struct wlr_xdg_toplevel* wlr_xdg_toplevel);
	~Toplevel();

	void focus();
	void begin_interactive(enum CursorMode mode, uint32_t edges);
};
