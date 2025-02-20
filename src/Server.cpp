#include "Server.h"
#include "wlr.h"
#include <libinput.h>
#include <thread>

// create a new keyboard
void Server::new_keyboard(struct wlr_input_device *device) {
    struct Keyboard *keyboard = new Keyboard(this, device);

    // connect to seat
    wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);

    // add to keyboards list
    wl_list_insert(&keyboards, &keyboard->link);
}

// create a new pointer
void Server::new_pointer(struct wlr_pointer *pointer) {
    struct libinput_device *device;

    if (wlr_input_device_is_libinput(&pointer->base) &&
        (device = wlr_libinput_get_device_handle(&pointer->base))) {

        if (libinput_device_config_tap_get_finger_count(device)) {
            // tap to click
            libinput_device_config_tap_set_enabled(device,
                                                   config->cursor.tap_to_click);

            // tap and drag
            libinput_device_config_tap_set_drag_enabled(
                device, config->cursor.tap_and_drag);

            // drag lock
            libinput_device_config_tap_set_drag_lock_enabled(
                device, config->cursor.drag_lock);

            // buttom map
            libinput_device_config_tap_set_button_map(
                device, config->cursor.tap_button_map);
        }

        // natural scroll
        if (libinput_device_config_scroll_has_natural_scroll(device))
            libinput_device_config_scroll_set_natural_scroll_enabled(
                device, config->cursor.natural_scroll);

        // disable while typing
        if (libinput_device_config_dwt_is_available(device))
            libinput_device_config_dwt_set_enabled(
                device, config->cursor.disable_while_typing);

        // left handed
        if (libinput_device_config_left_handed_is_available(device))
            libinput_device_config_left_handed_set(device,
                                                   config->cursor.left_handed);

        // middle emulation
        if (libinput_device_config_middle_emulation_is_available(device))
            libinput_device_config_middle_emulation_set_enabled(
                device, config->cursor.middle_emulation);

        // scroll method
        if (libinput_device_config_scroll_get_methods(device) !=
            LIBINPUT_CONFIG_SCROLL_NO_SCROLL)
            libinput_device_config_scroll_set_method(
                device, config->cursor.scroll_method);

        // click method
        if (libinput_device_config_click_get_methods(device) !=
            LIBINPUT_CONFIG_CLICK_METHOD_NONE)
            libinput_device_config_click_set_method(
                device, config->cursor.click_method);

        // event mode
        if (libinput_device_config_send_events_get_modes(device))
            libinput_device_config_send_events_set_mode(
                device, config->cursor.event_mode);

        if (libinput_device_config_accel_is_available(device)) {
            // profile
            libinput_device_config_accel_set_profile(device,
                                                     config->cursor.profile);

            // accel speed
            libinput_device_config_accel_set_speed(device,
                                                   config->cursor.accel_speed);
        }
    }

    // attach to device
    wlr_cursor_attach_input_device(cursor->cursor, &pointer->base);
}

// get workspace by toplevel
struct Workspace *Server::get_workspace(struct Toplevel *toplevel) {
    Output *output, *tmp;
    Workspace *workspace, *tmp1;

    // check each output
    // for each output check each workspace
    wl_list_for_each_safe(output, tmp, &output_manager->outputs, link)
        wl_list_for_each_safe(
            workspace, tmp1, &output->workspaces,
            link) if (workspace->contains(toplevel)) return workspace;

    // no workspace found
    return nullptr;
}

// get a node tree surface from its location and cast it to the generic
// type provided
template <typename T>
T *Server::surface_at(double lx, double ly, struct wlr_surface **surface,
                      double *sx, double *sy) {
    // get the scene node and ensure it's a buffer
    struct wlr_scene_node *node =
        wlr_scene_node_at(&scene->tree.node, lx, ly, sx, sy);
    if (!node || node->type != WLR_SCENE_NODE_BUFFER)
        return NULL;

    // get the scene buffer and surface of the node
    struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
    struct wlr_scene_surface *scene_surface =
        wlr_scene_surface_try_from_buffer(scene_buffer);
    if (!scene_surface || !scene_surface->surface)
        return NULL;

    // set the scene surface
    *surface = scene_surface->surface;

    // get the scene tree of the node's parent
    struct wlr_scene_tree *tree = node->parent;
    if (!tree || tree->node.type != WLR_SCENE_NODE_TREE)
        return NULL;

    // find the topmost node of the scene tree
    while (tree && tree->node.data == NULL)
        tree = tree->node.parent;

    // invalid tree
    if (!tree || !tree->node.parent)
        return NULL;

    // return the topmost node's data
    return (T *)(tree->node.data);
}

// find a toplevel by location
struct Toplevel *Server::toplevel_at(double lx, double ly,
                                     struct wlr_surface **surface, double *sx,
                                     double *sy) {
    Toplevel *toplevel = surface_at<Toplevel>(lx, ly, surface, sx, sy);

    // ensure role is not layer surface
    if (toplevel && surface && (*surface)->mapped &&
        strcmp((*surface)->role->name, "zwlr_layer_surface_v1") != 0)
        return toplevel;
    else
        return nullptr;
}

// find a layer surface by location
struct LayerSurface *Server::layer_surface_at(double lx, double ly,
                                              struct wlr_surface **surface,
                                              double *sx, double *sy) {
    LayerSurface *layer_surface =
        surface_at<LayerSurface>(lx, ly, surface, sx, sy);

    // ensure role is layer surface
    if (layer_surface && surface && (*surface)->mapped &&
        strcmp((*surface)->role->name, "zwlr_layer_surface_v1") == 0)
        return layer_surface;
    else
        return nullptr;
}

// get output by wlr_output
struct Output *Server::get_output(struct wlr_output *wlr_output) {
    return output_manager->get_output(wlr_output);
}

// get the focused output
struct Output *Server::focused_output() {
    return output_manager->output_at(cursor->cursor->x, cursor->cursor->y);
}

Server::Server(struct Config *config) {
    // set config from file
    this->config = config;

    // display
    wl_display = wl_display_create();

    // backend
    backend =
        wlr_backend_autocreate(wl_display_get_event_loop(wl_display), NULL);
    if (backend == NULL) {
        wlr_log(WLR_ERROR, "failed to create wlr_backend");
        ::exit(1);
    }

    // renderer
    renderer = wlr_renderer_autocreate(backend);
    if (renderer == NULL) {
        wlr_log(WLR_ERROR, "failed to create wlr_renderer");
        ::exit(1);
    }

    wlr_renderer_init_wl_shm(renderer, wl_display);

    // render allocator
    allocator = wlr_allocator_autocreate(backend, renderer);
    if (allocator == NULL) {
        wlr_log(WLR_ERROR, "failed to create wlr_allocator");
        ::exit(1);
    }

    // wlr compositor
    compositor = wlr_compositor_create(wl_display, 5, renderer);
    wlr_subcompositor_create(wl_display);
    wlr_data_device_manager_create(wl_display);

    // output manager
    output_manager = new OutputManager(this);

    // scene
    scene = wlr_scene_create();
    scene_layout =
        wlr_scene_attach_output_layout(scene, output_manager->layout);

    // create xdg shell
    xdg_shell = wlr_xdg_shell_create(wl_display, 6);

    // new_xdg_toplevel
    new_xdg_toplevel.notify = [](struct wl_listener *listener, void *data) {
        struct Server *server =
            wl_container_of(listener, server, new_xdg_toplevel);

        // toplevels are managed by workspaces
        [[maybe_unused]] struct Toplevel *toplevel =
            new Toplevel(server, static_cast<wlr_xdg_toplevel *>(data));
    };
    wl_signal_add(&xdg_shell->events.new_toplevel, &new_xdg_toplevel);

    // new_xdg_popup
    new_xdg_popup.notify = [](struct wl_listener *listener, void *data) {
        // popups do not need to be tracked
        [[maybe_unused]] struct Popup *popup =
            new Popup(static_cast<wlr_xdg_popup *>(data));
    };
    wl_signal_add(&xdg_shell->events.new_popup, &new_xdg_popup);

    // layers
    layers.background = wlr_scene_tree_create(&scene->tree);
    layers.bottom = wlr_scene_tree_create(&scene->tree);
    layers.floating = wlr_scene_tree_create(&scene->tree);
    layers.fullscreen = wlr_scene_tree_create(&scene->tree);
    layers.top = wlr_scene_tree_create(&scene->tree);
    layers.overlay = wlr_scene_tree_create(&scene->tree);

    // layer shell
    wl_list_init(&layer_surfaces);
    wlr_layer_shell = wlr_layer_shell_v1_create(wl_display, 5);

    // new_shell_surface
    new_shell_surface.notify = [](struct wl_listener *listener, void *data) {
        // layer surface created
        Server *server = wl_container_of(listener, server, new_shell_surface);
        wlr_layer_surface_v1 *surface =
            static_cast<wlr_layer_surface_v1 *>(data);

        Output *output = nullptr;

        // assume focused output if not set
        if (surface->output) {
            output = server->get_output(surface->output);
        } else {
            output = server->focused_output();

            if (output)
                surface->output = output->wlr_output;
            else {
                wlr_log(WLR_ERROR, "no available output for layer surface");
                return;
            }
        }

        // add to layer surfaces
        LayerSurface *layer_surface = new LayerSurface(output, surface);
        if (layer_surface)
            wl_list_insert(&server->layer_surfaces, &layer_surface->link);
    };
    wl_signal_add(&wlr_layer_shell->events.new_surface, &new_shell_surface);

    // renderer_lost
    renderer_lost.notify = [](struct wl_listener *listener, void *data) {
        // renderer recovery (thanks sway)
        struct Server *server =
            wl_container_of(listener, server, renderer_lost);

        wlr_log(WLR_INFO, "Re-creating renderer after GPU reset");

        // create new renderer
        struct wlr_renderer *renderer =
            wlr_renderer_autocreate(server->backend);
        if (renderer == NULL) {
            wlr_log(WLR_ERROR, "Unable to create renderer");
            return;
        }

        // create new allocator
        struct wlr_allocator *allocator =
            wlr_allocator_autocreate(server->backend, renderer);
        if (allocator == NULL) {
            wlr_log(WLR_ERROR, "Unable to create allocator");
            wlr_renderer_destroy(renderer);
            return;
        }

        // replace old and renderer and allocator
        struct wlr_renderer *old_renderer = server->renderer;
        struct wlr_allocator *old_allocator = server->allocator;
        server->renderer = renderer;
        server->allocator = allocator;

        // reset signal
        wl_list_remove(&server->renderer_lost.link);
        wl_signal_add(&server->renderer->events.lost, &server->renderer_lost);

        // move compositor to new renderer
        wlr_compositor_set_renderer(server->compositor, renderer);

        // reinint outputs
        struct Output *output, *tmp;
        wl_list_for_each_safe(output, tmp, &server->output_manager->outputs,
                              link)
            wlr_output_init_render(output->wlr_output, server->allocator,
                                   server->renderer);

        // destroy old renderer and allocator
        wlr_allocator_destroy(old_allocator);
        wlr_renderer_destroy(old_renderer);
    };
    wl_signal_add(&renderer->events.lost, &renderer_lost);

    // cursor
    cursor = new Cursor(this);

    // keyboards
    wl_list_init(&keyboards);

    // new_input
    new_input.notify = [](struct wl_listener *listener, void *data) {
        // create input device based on type
        struct Server *server = wl_container_of(listener, server, new_input);
        struct wlr_input_device *device = static_cast<wlr_input_device *>(data);

        // handle device type
        switch (device->type) {
        case WLR_INPUT_DEVICE_KEYBOARD:
            server->new_keyboard(device);
            break;
        case WLR_INPUT_DEVICE_POINTER:
            server->new_pointer((wlr_pointer *)device);
            break;
        default:
            break;
        }

        // set input device capabilities
        uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
        if (!wl_list_empty(&server->keyboards))
            caps |= WL_SEAT_CAPABILITY_KEYBOARD;

        wlr_seat_set_capabilities(server->seat, caps);
    };
    wl_signal_add(&backend->events.new_input, &new_input);

    // seat
    seat = wlr_seat_create(wl_display, "seat0");

    // request_cursor (seat)
    request_cursor.notify = [](struct wl_listener *listener, void *data) {
        // client-provided cursor image
        struct Server *server =
            wl_container_of(listener, server, request_cursor);

        struct wlr_seat_pointer_request_set_cursor_event *event =
            static_cast<wlr_seat_pointer_request_set_cursor_event *>(data);
        struct wlr_seat_client *focused_client =
            server->seat->pointer_state.focused_client;

        // only obey focused client
        if (focused_client == event->seat_client)
            wlr_cursor_set_surface(server->cursor->cursor, event->surface,
                                   event->hotspot_x, event->hotspot_y);
    };
    wl_signal_add(&seat->events.request_set_cursor, &request_cursor);

    // request_set_selection (seat)
    request_set_selection.notify = [](struct wl_listener *listener,
                                      void *data) {
        // user selection
        struct Server *server =
            wl_container_of(listener, server, request_set_selection);

        struct wlr_seat_request_set_selection_event *event =
            static_cast<wlr_seat_request_set_selection_event *>(data);

        wlr_seat_set_selection(server->seat, event->source, event->serial);
    };
    wl_signal_add(&seat->events.request_set_selection, &request_set_selection);

    // virtual pointer manager
    virtual_pointer_mgr = wlr_virtual_pointer_manager_v1_create(wl_display);

    new_virtual_pointer.notify = [](struct wl_listener *listener, void *data) {
        Server *server = wl_container_of(listener, server, new_virtual_pointer);

        struct wlr_virtual_pointer_v1_new_pointer_event *event =
            static_cast<wlr_virtual_pointer_v1_new_pointer_event *>(data);
        struct wlr_virtual_pointer_v1 *pointer = event->new_pointer;
        struct wlr_input_device *device = &pointer->pointer.base;

        wlr_cursor_attach_input_device(server->cursor->cursor, device);
        if (event->suggested_output)
            wlr_cursor_map_input_to_output(server->cursor->cursor, device,
                                           event->suggested_output);
    };
    wl_signal_add(&virtual_pointer_mgr->events.new_virtual_pointer,
                  &new_virtual_pointer);

    // xwayland shell
    // xwayland_shell = new XWaylandShell(wl_display, scene);

    // export dmabuf manager
    wlr_export_dmabuf_manager = wlr_export_dmabuf_manager_v1_create(wl_display);

    // screencopy manager
    wlr_screencopy_manager = wlr_screencopy_manager_v1_create(wl_display);

    // foreign toplevel list
    wlr_foreign_toplevel_list =
        wlr_ext_foreign_toplevel_list_v1_create(wl_display, 1);

    // foreign toplevel manager
    wlr_foreign_toplevel_manager =
        wlr_foreign_toplevel_manager_v1_create(wl_display);

    // data control manager
    wlr_data_control_manager = wlr_data_control_manager_v1_create(wl_display);

    // gamma control manager
    wlr_gamma_control_manager = wlr_gamma_control_manager_v1_create(wl_display);
    wlr_scene_set_gamma_control_manager_v1(scene, wlr_gamma_control_manager);

    // image copy capture manager
    ext_image_copy_capture_manager =
        wlr_ext_image_copy_capture_manager_v1_create(wl_display, 1);
    wlr_ext_output_image_capture_source_manager_v1_create(wl_display, 1);

    // fractional scale manager
    wlr_fractional_scale_manager =
        wlr_fractional_scale_manager_v1_create(wl_display, 1);

    // drm syncobj manager
    if (wlr_renderer_get_drm_fd(renderer) >= 0 && renderer->features.timeline &&
        backend->features.timeline) {
        wlr_linux_drm_syncobj_manager_v1_create(
            wl_display, 1, wlr_renderer_get_drm_fd(renderer));
    }

    // avoid using "wayland-0" as display socket
    std::string socket;
    for (unsigned int i = 1; i <= 32; i++) {
        socket = "wayland-" + std::to_string(i);
        int ret = wl_display_add_socket(wl_display, socket.c_str());
        if (!ret)
            break;
        else
            wlr_log(WLR_ERROR,
                    "wl_display_add_socket for %s returned %d: skipping",
                    socket.c_str(), ret);
    }

    if (socket.empty()) {
        wlr_log(WLR_DEBUG, "Unable to open wayland socket");
        wlr_backend_destroy(backend);
        return;
    }

    // backend start
    if (!wlr_backend_start(backend)) {
        wlr_backend_destroy(backend);
        wl_display_destroy(wl_display);
        ::exit(1);
    }

    // linux dmabuf
    if (wlr_renderer_get_texture_formats(renderer, WLR_BUFFER_CAP_DMABUF)) {
        wlr_drm_create(wl_display, renderer);
        wlr_linux_dmabuf =
            wlr_linux_dmabuf_v1_create_with_renderer(wl_display, 4, renderer);
        wlr_scene_set_linux_dmabuf_v1(scene, wlr_linux_dmabuf);
    }

    // set up signal handler
    struct sigaction sa;
    sa.sa_handler = [](int sig) {
        if (sig == SIGCHLD)
            while (waitpid(-1, NULL, WNOHANG) > 0)
                ;
        else if (sig == SIGINT || sig == SIGTERM)
            Server::get().exit();
    };
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // set wayland diplay to our socket
    setenv("WAYLAND_DISPLAY", socket.c_str(), true);

    // set xdg current desktop for portals
    setenv("XDG_CURRENT_DESKTOP", "awm", true);

    // set envvars from config
    for (std::pair<std::string, std::string> kv : config->startup_env)
        setenv(kv.first.c_str(), kv.second.c_str(), true);

    // run startup commands from config
    for (std::string command : config->startup_commands)
        if (fork() == 0)
            execl("/bin/sh", "/bin/sh", "-c", command.c_str(), nullptr);

    // run thread for config updater
    std::thread config_thread([&]() {
        while (true) {
            // update config
            config->update(this);

            // sleep
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
    config_thread.detach();

    // run event loop
    wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s",
            socket.c_str());
    wl_display_run(wl_display);
}

void Server::exit() {
    wl_display_terminate(wl_display);

    // run exit commands
    for (std::string command : config->exit_commands)
        if (fork() == 0)
            execl("/bin/sh", "/bin/sh", "-c", command.c_str(), nullptr);
}

Server::~Server() {
    wl_display_destroy_clients(wl_display);

    delete output_manager;

    wl_list_remove(&new_xdg_toplevel.link);
    wl_list_remove(&new_xdg_popup.link);

    delete cursor;

    wl_list_remove(&new_input.link);
    wl_list_remove(&request_cursor.link);
    wl_list_remove(&request_set_selection.link);

    wl_list_remove(&renderer_lost.link);

    wl_list_remove(&new_shell_surface.link);
    wl_list_remove(&new_virtual_pointer.link);

    struct LayerSurface *surface, *tmp;
    wl_list_for_each_safe(surface, tmp, &layer_surfaces, link) delete surface;
    // delete xwayland_shell;

    wlr_scene_node_destroy(&scene->tree.node);
    wlr_allocator_destroy(allocator);
    wlr_renderer_destroy(renderer);
    wlr_backend_destroy(backend);
    wl_display_destroy(wl_display);
}
