#include <vector>

#include "wlr.h"
#include "Output.h"
#include "Toplevel.h"
#include "Keyboard.h"

enum cursor_mode {
    passthrough,
    move,
    resize
};

class Server {
    wl_display *display;
    wlr_backend *backend;
    wlr_renderer *renderer;
    wlr_allocator *allocator;
    wlr_scene *scene;
    wlr_scene_output_layout *scene_layout;

    wlr_xdg_shell *xdg_shell;
	wl_listener new_xdg_toplevel;
	wl_listener new_xdg_popup;
	std::vector<Toplevel*> toplevels;

	wlr_cursor *cursor;
	wlr_xcursor_manager *cursor_mgr;
	wl_listener cursor_motion;
	wl_listener cursor_motion_absolute;
	wl_listener cursor_button;
	wl_listener cursor_axis;
	wl_listener cursor_frame;

	wlr_seat *seat;
	wl_listener new_input;
	wl_listener request_cursor;
	wl_listener request_set_selection;
	std::vector<Keyboard*> keyboards;
	cursor_mode mode;
	Toplevel *grabbed_toplevel;
	double grab_x, grab_y;
	wlr_box grab_geobox;
	uint32_t resize_edges;

	wlr_output_layout *output_layout;
	std::vector<Output*> outputs;
	wl_listener output_new;
public:
	Server();
	~Server();

	void focus_toplevel(Toplevel *toplevel);
	bool handle_keybinding(xkb_keysym_t sym);
	void new_keyboard(wlr_input_device *device);
	void new_pointer(wlr_input_device *device);
	Toplevel *desktop_toplevel_at(double lx, double ly, wlr_surface **surface, double *sx, double *sy);
	void reset_cursor_mode();
	void process_cursor_move();
	void process_cursor_resize();
	void process_cursor_motion(uint32_t time);
	void begin_interactive(Toplevel *toplevel, cursor_mode mode, uint32_t edges);
};
