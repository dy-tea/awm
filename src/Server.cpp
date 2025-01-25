#include "Server.h"

void Server::new_keyboard(struct wlr_input_device *device) {
	struct Keyboard *keyboard = new Keyboard(this, device);

	wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);

	/* And add the keyboard to our list of keyboards */
	wl_list_insert(&keyboards, &keyboard->link);
}

void Server::new_pointer(struct wlr_input_device *device) {
	/* We don't do anything special with pointers. All of our pointer handling
	 * is proxied through wlr_cursor. On another compositor, you might take this
	 * opportunity to do libinput configuration on the device to set
	 * acceleration, etc. */
	wlr_cursor_attach_input_device(cursor, device);
}

// TODO: maybe move to toplevel
static struct Toplevel *desktop_toplevel_at(
		struct Server *server, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	/* This returns the topmost node in the scene at the given layout coords.
	 * We only care about surface nodes as we are specifically looking for a
	 * surface in the surface tree of a Toplevel. */
	struct wlr_scene_node *node = wlr_scene_node_at(
		&server->scene->tree.node, lx, ly, sx, sy);
	if (node == NULL || node->type != WLR_SCENE_NODE_BUFFER) {
		return NULL;
	}
	struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(scene_buffer);
	if (!scene_surface) {
		return NULL;
	}

	*surface = scene_surface->surface;
	/* Find the node corresponding to the Toplevel at the root of this
	 * surface tree, it is the only one for which we set the data field. */
	struct wlr_scene_tree *tree = node->parent;
	while (tree != NULL && tree->node.data == NULL) {
		tree = tree->node.parent;
	}
	return (Toplevel*)tree->node.data;
}

void Server::reset_cursor_mode() {
	/* Reset the cursor mode to passthrough. */
	cursor_mode = CURSORMODE_PASSTHROUGH;
	grabbed_toplevel = NULL;
}

void Server::process_cursor_move() {
	/* Move the grabbed toplevel to the new position. */
	wlr_scene_node_set_position(&grabbed_toplevel->scene_tree->node, cursor->x - grab_x, cursor->y - grab_y);
}

void Server::process_cursor_resize() {
	/*
	 * Resizing the grabbed toplevel can be a little bit complicated, because we
	 * could be resizing from any corner or edge. This not only resizes the
	 * toplevel on one or two axes, but can also move the toplevel if you resize
	 * from the top or left edges (or top-left corner).
	 *
	 * Note that some shortcuts are taken here. In a more fleshed-out
	 * compositor, you'd wait for the client to prepare a buffer at the new
	 * size, then commit any movement that was prepared.
	 */
	struct Toplevel *toplevel = grabbed_toplevel;
	double border_x = cursor->x - grab_x;
	double border_y = cursor->y - grab_y;
	int new_left = grab_geobox.x;
	int new_right = grab_geobox.x + grab_geobox.width;
	int new_top = grab_geobox.y;
	int new_bottom = grab_geobox.y + grab_geobox.height;

	if (resize_edges & WLR_EDGE_TOP) {
		new_top = border_y;
		if (new_top >= new_bottom)
			new_top = new_bottom - 1;
	} else if (resize_edges & WLR_EDGE_BOTTOM) {
		new_bottom = border_y;
		if (new_bottom <= new_top)
			new_bottom = new_top + 1;
	}
	if (resize_edges & WLR_EDGE_LEFT) {
		new_left = border_x;
		if (new_left >= new_right)
			new_left = new_right - 1;
	} else if (resize_edges & WLR_EDGE_RIGHT) {
		new_right = border_x;
		if (new_right <= new_left)
			new_right = new_left + 1;
	}

	struct wlr_box *geo_box = &toplevel->xdg_toplevel->base->geometry;
	wlr_scene_node_set_position(&toplevel->scene_tree->node,
		new_left - geo_box->x, new_top - geo_box->y);

	wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, new_right - new_left, new_bottom - new_top);
}

void Server::process_cursor_motion(uint32_t time) {
	/* If the mode is non-passthrough, delegate to those functions. */
	if (cursor_mode == CURSORMODE_MOVE) {
		process_cursor_move();
		return;
	} else if (cursor_mode == CURSORMODE_RESIZE) {
		process_cursor_resize();
		return;
	}

	/* Otherwise, find the toplevel under the pointer and send the event along. */
	double sx, sy;
	struct wlr_surface *surface = NULL;
	struct Toplevel *toplevel = desktop_toplevel_at(this, cursor->x, cursor->y, &surface, &sx, &sy);
	if (!toplevel) {
		/* If there's no toplevel under the cursor, set the cursor image to a
		 * default. This is what makes the cursor image appear when you move it
		 * around the screen, not over any toplevels. */
		wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");
	}
	if (surface) {
		/*
		 * Send pointer enter and motion events.
		 *
		 * The enter event gives the surface "pointer focus", which is distinct
		 * from keyboard focus. You get pointer focus by moving the pointer over
		 * a window.
		 *
		 * Note that wlroots will avoid sending duplicate enter/motion events if
		 * the surface has already has pointer focus or if the client is already
		 * aware of the coordinates passed.
		 */
		wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
		wlr_seat_pointer_notify_motion(seat, time, sx, sy);
	} else {
		/* Clear pointer focus so future button events and such are not sent to
		 * the last client to have the cursor over it. */
		wlr_seat_pointer_clear_focus(seat);
	}
}

void begin_interactive(struct Toplevel *toplevel,
		enum CursorMode mode, uint32_t edges) {
	/* This function sets up an interactive move or resize operation, where the
	 * compositor stops propegating pointer events to clients and instead
	 * consumes them itself, to move or resize windows. */
	struct Server *server = toplevel->server;

	server->grabbed_toplevel = toplevel;
	server->cursor_mode = mode;

	if (mode == CURSORMODE_MOVE) {
		server->grab_x = server->cursor->x - toplevel->scene_tree->node.x;
		server->grab_y = server->cursor->y - toplevel->scene_tree->node.y;
	} else {
		struct wlr_box *geo_box = &toplevel->xdg_toplevel->base->geometry;

		double border_x = (toplevel->scene_tree->node.x + geo_box->x) +
			((edges & WLR_EDGE_RIGHT) ? geo_box->width : 0);
		double border_y = (toplevel->scene_tree->node.y + geo_box->y) +
			((edges & WLR_EDGE_BOTTOM) ? geo_box->height : 0);
		server->grab_x = server->cursor->x - border_x;
		server->grab_y = server->cursor->y - border_y;

		server->grab_geobox = *geo_box;
		server->grab_geobox.x += toplevel->scene_tree->node.x;
		server->grab_geobox.y += toplevel->scene_tree->node.y;

		server->resize_edges = edges;
	}
}

Server::Server(const char* startup_cmd) {
    /* The Wayland display is managed by libwayland. It handles accepting
	 * clients from the Unix socket, manging Wayland globals, and so on. */
	wl_display = wl_display_create();
	/* The backend is a wlroots feature which abstracts the underlying input and
	 * output hardware. The autocreate option will choose the most suitable
	 * backend based on the current environment, such as opening an X11 window
	 * if an X11 server is running. */
	backend = wlr_backend_autocreate(wl_display_get_event_loop(wl_display), NULL);
	if (backend == NULL) {
		wlr_log(WLR_ERROR, "failed to create wlr_backend");
		exit(1);
	}

	/* Autocreates a renderer, either Pixman, GLES2 or Vulkan for us. The user
	 * can also specify a renderer using the WLR_RENDERER env var.
	 * The renderer is responsible for defining the various pixel formats it
	 * supports for shared memory, this configures that for clients. */
	renderer = wlr_renderer_autocreate(backend);
	if (renderer == NULL) {
		wlr_log(WLR_ERROR, "failed to create wlr_renderer");
		exit(1);
	}

	wlr_renderer_init_wl_display(renderer, wl_display);

	/* Autocreates an allocator for us.
	 * The allocator is the bridge between the renderer and the backend. It
	 * handles the buffer creation, allowing wlroots to render onto the
	 * screen */
	allocator = wlr_allocator_autocreate(backend,
		renderer);
	if (allocator == NULL) {
		wlr_log(WLR_ERROR, "failed to create wlr_allocator");
		exit(1);
	}

	/* This creates some hands-off wlroots interfaces. The compositor is
	 * necessary for clients to allocate surfaces, the subcompositor allows to
	 * assign the role of subsurfaces to surfaces and the data device manager
	 * handles the clipboard. Each of these wlroots interfaces has room for you
	 * to dig your fingers in and play with their behavior if you want. Note that
	 * the clients cannot set the selection directly without compositor approval,
	 * see the handling of the request_set_selection event below.*/
	wlr_compositor_create(wl_display, 5, renderer);
	wlr_subcompositor_create(wl_display);
	wlr_data_device_manager_create(wl_display);

	/* Creates an output layout, which a wlroots utility for working with an
	 * arrangement of screens in a physical layout. */
	output_layout = wlr_output_layout_create(wl_display);

	/* Configure a listener to be notified when new outputs are available on the
	 * backend. */
	wl_list_init(&outputs);

	// new_output
	new_output.notify = [](struct wl_listener *listener, void *data) {
		/* This event is raised by the backend when a new output (aka a display or
		 * monitor) becomes available. */
		struct Server *server =
			wl_container_of(listener, server, new_output);
		struct wlr_output *wlr_output = data;

		/* Configures the output created by the backend to use our allocator
		 * and our renderer. Must be done once, before commiting the output */
		wlr_output_init_render(wlr_output, server->allocator, server->renderer);

		/* The output may be disabled, switch it on. */
		struct wlr_output_state state;
		wlr_output_state_init(&state);
		wlr_output_state_set_enabled(&state, true);

		/* Some backends don't have modes. DRM+KMS does, and we need to set a mode
		 * before we can use the output. The mode is a tuple of (width, height,
		 * refresh rate), and each monitor supports only a specific set of modes. We
		 * just pick the monitor's preferred mode, a more sophisticated compositor
		 * would let the user configure it. */
		struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
		if (mode != NULL) {
			wlr_output_state_set_mode(&state, mode);
		}

		/* Atomically applies the new output state. */
		wlr_output_commit_state(wlr_output, &state);
		wlr_output_state_finish(&state);

		/* Allocates and configures our state for this output */
		struct Output *output = new Output;
		output->wlr_output = wlr_output;
		output->server = server;

		/* Sets up a listener for the frame event. */
		output->frame.notify = [](struct wl_listener *listener, void *data) {
			/* This function is called every time an output is ready to display a frame,
			 * generally at the output's refresh rate (e.g. 60Hz). */
			struct Output *output = wl_container_of(listener, output, frame);
			struct wlr_scene *scene = output->server->scene;

			struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(
				scene, output->wlr_output);

			/* Render the scene if needed and commit the output */
			wlr_scene_output_commit(scene_output, NULL);

			struct timespec now;
			clock_gettime(CLOCK_MONOTONIC, &now);
			wlr_scene_output_send_frame_done(scene_output, &now);
		};
		wl_signal_add(&wlr_output->events.frame, &output->frame);

		/* Sets up a listener for the state request event. */
		output->request_state.notify = [](struct wl_listener *listener, void *data) {
			/* This function is called when the backend requests a new state for
			 * the output. For example, Wayland and X11 backends request a new mode
			 * when the output window is resized. */
			struct Output *output = wl_container_of(listener, output, request_state);
			const struct wlr_output_event_request_state *event = (wlr_output_event_request_state*)data;
			wlr_output_commit_state(output->wlr_output, event->state);
		 };
		wl_signal_add(&wlr_output->events.request_state, &output->request_state);

		/* Sets up a listener for the destroy event. */
		output->destroy.notify = [](struct wl_listener *listener, void *data) {
			struct Output *output = wl_container_of(listener, output, destroy);

			wl_list_remove(&output->frame.link);
			wl_list_remove(&output->request_state.link);
			wl_list_remove(&output->destroy.link);
			wl_list_remove(&output->link);
			free(output);
		};
		wl_signal_add(&wlr_output->events.destroy, &output->destroy);

		wl_list_insert(&server->outputs, &output->link);

		/* Adds this to the output layout. The add_auto function arranges outputs
		 * from left-to-right in the order they appear. A more sophisticated
		 * compositor would let the user configure the arrangement of outputs in the
		 * layout.
		 *
		 * The output layout utility automatically adds a wl_output global to the
		 * display, which Wayland clients can see to find out information about the
		 * output (such as DPI, scale factor, manufacturer, etc).
		 */
		struct wlr_output_layout_output *l_output = wlr_output_layout_add_auto(server->output_layout,
			wlr_output);
		struct wlr_scene_output *scene_output = wlr_scene_output_create(server->scene, wlr_output);
		wlr_scene_output_layout_add_output(server->scene_layout, l_output, scene_output);;
	};
	wl_signal_add(&backend->events.new_output, &new_output);

	/* Create a scene graph. This is a wlroots abstraction that handles all
	 * rendering and damage tracking. All the compositor author needs to do
	 * is add things that should be rendered to the scene graph at the proper
	 * positions and then call wlr_scene_output_commit() to render a frame if
	 * necessary.
	 */
	scene = wlr_scene_create();
	scene_layout = wlr_scene_attach_output_layout(scene, output_layout);

	/* Set up xdg-shell version 3. The xdg-shell is a Wayland protocol which is
	 * used for application windows. For more detail on shells, refer to
	 * https://drewdevault.com/2018/07/29/Wayland-shells.html.
	 */
	wl_list_init(&toplevels);
	xdg_shell = wlr_xdg_shell_create(wl_display, 3);

	// new_xdg_toplevel
	new_xdg_toplevel.notify = [](struct wl_listener *listener, void *data) {
		/* This event is raised when a client creates a new toplevel (application window). */
		struct Server *server = wl_container_of(listener, server, new_xdg_toplevel);
		struct wlr_xdg_toplevel *xdg_toplevel = (wlr_xdg_toplevel*)data;

		/* Allocate a Toplevel for this surface */
		struct Toplevel *toplevel = new Toplevel(server, xdg_toplevel);
	};
	wl_signal_add(&xdg_shell->events.new_toplevel, &new_xdg_toplevel);

	// new_xdg_popup
	new_xdg_popup.notify = [](struct wl_listener *listener, void *data) {
		/* This event is raised when a client creates a new popup. */
		struct wlr_xdg_popup *xdg_popup = (wlr_xdg_popup*)data;

		struct Popup *popup = new Popup;
		popup->xdg_popup = xdg_popup;

		/* We must add xdg popups to the scene graph so they get rendered. The
		 * wlroots scene graph provides a helper for this, but to use it we must
		 * provide the proper parent scene node of the xdg popup. To enable this,
		 * we always set the user data field of xdg_surfaces to the corresponding
		 * scene node. */
		struct wlr_xdg_surface *parent = wlr_xdg_surface_try_from_wlr_surface(xdg_popup->parent);
		assert(parent != NULL);
		struct wlr_scene_tree *parent_tree = (wlr_scene_tree*)parent->data;
		xdg_popup->base->data = wlr_scene_xdg_surface_create(parent_tree, xdg_popup->base);

		// xdg_popup_commit
		popup->commit.notify = [](struct wl_listener *listener, void *data) {
			/* Called when a new surface state is committed. */
			struct Popup *popup = wl_container_of(listener, popup, commit);

			if (popup->xdg_popup->base->initial_commit) {
				/* When an xdg_surface performs an initial commit, the compositor must
				 * reply with a configure so the client can map the surface.
				 * tinywl sends an empty configure. A more sophisticated compositor
				 * might change an xdg_popup's geometry to ensure it's not positioned
				 * off-screen, for example. */
				wlr_xdg_surface_schedule_configure(popup->xdg_popup->base);
			}
		};
		wl_signal_add(&xdg_popup->base->surface->events.commit, &popup->commit);

		// xdg_popup_destroy
		popup->destroy.notify = [](struct wl_listener *listener, void *data) {
			/* Called when the xdg_popup is destroyed. */
			struct Popup *popup = wl_container_of(listener, popup, destroy);

			wl_list_remove(&popup->commit.link);
			wl_list_remove(&popup->destroy.link);

			free(popup);
		};
		wl_signal_add(&xdg_popup->events.destroy, &popup->destroy);
	};
	wl_signal_add(&xdg_shell->events.new_popup, &new_xdg_popup);

	/*
	 * Creates a cursor, which is a wlroots utility for tracking the cursor
	 * image shown on screen.
	 */
	cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(cursor, output_layout);

	/* Creates an xcursor manager, another wlroots utility which loads up
	 * Xcursor themes to source cursor images from and makes sure that cursor
	 * images are available at all scale factors on the screen (necessary for
	 * HiDPI support). */
	cursor_mgr = wlr_xcursor_manager_create(NULL, 24);

	/*
	 * wlr_cursor *only* displays an image on screen. It does not move around
	 * when the pointer moves. However, we can attach input devices to it, and
	 * it will generate aggregate events for all of them. In these events, we
	 * can choose how we want to process them, forwarding them to clients and
	 * moving the cursor around. More detail on this process is described in
	 * https://drewdevault.com/2018/07/17/Input-handling-in-wlroots.html.
	 *
	 * And more comments are sprinkled throughout the notify functions above.
	 */
	cursor_mode = CURSORMODE_PASSTHROUGH;

	// cursor_motion
	cursor_motion.notify = [](struct wl_listener *listener, void *data) {
		/* This event is forwarded by the cursor when a pointer emits a _relative_
		 * pointer motion event (i.e. a delta) */
		struct Server *server =
			wl_container_of(listener, server, cursor_motion);
		struct wlr_pointer_motion_event *event = (wlr_pointer_motion_event*)data;
		/* The cursor doesn't move unless we tell it to. The cursor automatically
		 * handles constraining the motion to the output layout, as well as any
		 * special configuration applied for the specific input device which
		 * generated the event. You can pass NULL for the device if you want to move
		 * the cursor around without any input. */
		wlr_cursor_move(server->cursor, &event->pointer->base,
				event->delta_x, event->delta_y);
		server->process_cursor_motion(event->time_msec);
	};
	wl_signal_add(&cursor->events.motion, &cursor_motion);

	// cursor_motion_absolute
	cursor_motion_absolute.notify = [](struct wl_listener *listener, void *data) {
		/* This event is forwarded by the cursor when a pointer emits an _absolute_
		 * motion event, from 0..1 on each axis. This happens, for example, when
		 * wlroots is running under a Wayland window rather than KMS+DRM, and you
		 * move the mouse over the window. You could enter the window from any edge,
		 * so we have to warp the mouse there. There is also some hardware which
		 * emits these events. */
		struct Server *server =
			wl_container_of(listener, server, cursor_motion_absolute);
		struct wlr_pointer_motion_absolute_event *event = (wlr_pointer_motion_absolute_event*)data;
		wlr_cursor_warp_absolute(server->cursor, &event->pointer->base, event->x,
			event->y);
		server->process_cursor_motion(event->time_msec);
	};
	wl_signal_add(&cursor->events.motion_absolute,
			&cursor_motion_absolute);

	// cursor_button
	cursor_button.notify = [](struct wl_listener *listener, void *data) {
		/* This event is forwarded by the cursor when a pointer emits a button
		 * event. */
		struct Server *server =
			wl_container_of(listener, server, cursor_button);
		struct wlr_pointer_button_event *event = (wlr_pointer_button_event*)data;
		/* Notify the client with pointer focus that a button press has occurred */
		wlr_seat_pointer_notify_button(server->seat,
				event->time_msec, event->button, event->state);
		if (event->state == WL_POINTER_BUTTON_STATE_RELEASED) {
			/* If you released any buttons, we exit interactive move/resize mode. */
			server->reset_cursor_mode();
		} else {
			/* Focus that client if the button was _pressed_ */
			double sx, sy;
			struct wlr_surface *surface = NULL;
			struct Toplevel *toplevel = desktop_toplevel_at(server,
					server->cursor->x, server->cursor->y, &surface, &sx, &sy);
			toplevel->focus();
		}
	};
	wl_signal_add(&cursor->events.button, &cursor_button);

	// cursor_axis
	cursor_axis.notify = [](struct wl_listener *listener, void *data) {
		/* This event is forwarded by the cursor when a pointer emits an axis event,
		 * for example when you move the scroll wheel. */
		struct Server *server =
			wl_container_of(listener, server, cursor_axis);
		struct wlr_pointer_axis_event *event = (wlr_pointer_axis_event*)data;
		/* Notify the client with pointer focus of the axis event. */
		wlr_seat_pointer_notify_axis(server->seat,
				event->time_msec, event->orientation, event->delta,
				event->delta_discrete, event->source, event->relative_direction);
	};
	wl_signal_add(&cursor->events.axis, &cursor_axis);

	// cursor_frame
	cursor_frame.notify = [](struct wl_listener *listener, void *data) {
		/* This event is forwarded by the cursor when a pointer emits an frame
		 * event. Frame events are sent after regular pointer events to group
		 * multiple events together. For instance, two axis events may happen at the
		 * same time, in which case a frame event won't be sent in between. */
		struct Server *server =
			wl_container_of(listener, server, cursor_frame);
		/* Notify the client with pointer focus of the frame event. */
		wlr_seat_pointer_notify_frame(server->seat);
	};
	wl_signal_add(&cursor->events.frame, &cursor_frame);

	/*
	 * Configures a seat, which is a single "seat" at which a user sits and
	 * operates the computer. This conceptually includes up to one keyboard,
	 * pointer, touch, and drawing tablet device. We also rig up a listener to
	 * let us know when new input devices are available on the backend.
	 */
	wl_list_init(&keyboards);

	// new_input
	new_input.notify = [](struct wl_listener *listener, void *data) {
		/* This event is raised by the backend when a new input device becomes
		 * available. */
		struct Server *server =
			wl_container_of(listener, server, new_input);
		struct wlr_input_device *device = (wlr_input_device*)data;
		switch (device->type) {
		case WLR_INPUT_DEVICE_KEYBOARD:
			server->new_keyboard(device);
			break;
		case WLR_INPUT_DEVICE_POINTER:
			server->new_pointer(device);
			break;
		default:
			break;
		}
		/* We need to let the wlr_seat know what our capabilities are, which is
		 * communiciated to the client. In TinyWL we always have a cursor, even if
		 * there are no pointer devices, so we always include that capability. */
		uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
		if (!wl_list_empty(&server->keyboards)) {
			caps |= WL_SEAT_CAPABILITY_KEYBOARD;
		}
		wlr_seat_set_capabilities(server->seat, caps);
	};
	wl_signal_add(&backend->events.new_input, &new_input);

	seat = wlr_seat_create(wl_display, "seat0");

	// request_cursor (seat)
	request_cursor.notify = [](struct wl_listener *listener, void *data) {
		struct Server *server = wl_container_of(
				listener, server, request_cursor);
		/* This event is raised by the seat when a client provides a cursor image */
		struct wlr_seat_pointer_request_set_cursor_event *event = (wlr_seat_pointer_request_set_cursor_event*)data;
		struct wlr_seat_client *focused_client =
			server->seat->pointer_state.focused_client;
		/* This can be sent by any client, so we check to make sure this one is
		 * actually has pointer focus first. */
		if (focused_client == event->seat_client) {
			/* Once we've vetted the client, we can tell the cursor to use the
			 * provided surface as the cursor image. It will set the hardware cursor
			 * on the output that it's currently on and continue to do so as the
			 * cursor moves between outputs. */
			wlr_cursor_set_surface(server->cursor, event->surface,
					event->hotspot_x, event->hotspot_y);
		}
	};
	wl_signal_add(&seat->events.request_set_cursor,
			&request_cursor);

	// request_set_selection (seat)
	request_set_selection.notify = [](struct wl_listener *listener, void *data) {
		/* This event is raised by the seat when a client wants to set the selection,
		 * usually when the user copies something. wlroots allows compositors to
		 * ignore such requests if they so choose, but in tinywl we always honor
		 */
		struct Server *server = wl_container_of(
				listener, server, request_set_selection);
		struct wlr_seat_request_set_selection_event *event = (wlr_seat_request_set_selection_event*)data;
		wlr_seat_set_selection(server->seat, event->source, event->serial);
	};
	wl_signal_add(&seat->events.request_set_selection,
			&request_set_selection);

	/* Add a Unix socket to the Wayland display. */
	const char *socket = wl_display_add_socket_auto(wl_display);
	if (!socket) {
		wlr_backend_destroy(backend);
		exit(1);
	}

	/* Start the backend. This will enumerate outputs and inputs, become the DRM
	 * master, etc */
	if (!wlr_backend_start(backend)) {
		wlr_backend_destroy(backend);
		wl_display_destroy(wl_display);
		exit(1);
	}

	/* Set the WAYLAND_DISPLAY environment variable to our socket and run the
	 * startup command if requested. */
	setenv("WAYLAND_DISPLAY", socket, true);
	if (startup_cmd) {
		if (fork() == 0) {
			execl("/bin/sh", "/bin/sh", "-c", startup_cmd, (void *)NULL);
		}
	}
	/* Run the Wayland event loop. This does not return until you exit the
	 * compositor. Starting the backend rigged up all of the necessary event
	 * loop configuration to listen to libinput events, DRM events, generate
	 * frame events at the refresh rate, and so on. */
	wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s",
			socket);
	wl_display_run(wl_display);
}

Server::~Server() {
	wl_display_destroy_clients(wl_display);

	wl_list_remove(&new_xdg_toplevel.link);
	wl_list_remove(&new_xdg_popup.link);

	wl_list_remove(&cursor_motion.link);
	wl_list_remove(&cursor_motion_absolute.link);
	wl_list_remove(&cursor_button.link);
	wl_list_remove(&cursor_axis.link);
	wl_list_remove(&cursor_frame.link);

	wl_list_remove(&new_input.link);
	wl_list_remove(&request_cursor.link);
	wl_list_remove(&request_set_selection.link);

	wl_list_remove(&new_output.link);

	wlr_scene_node_destroy(&scene->tree.node);
	wlr_xcursor_manager_destroy(cursor_mgr);
	wlr_cursor_destroy(cursor);
	wlr_allocator_destroy(allocator);
	wlr_renderer_destroy(renderer);
	wlr_backend_destroy(backend);
	wl_display_destroy(wl_display);
}
