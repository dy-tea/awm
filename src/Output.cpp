#include "Server.h"

Output::Output(struct Server *server, struct wlr_output *wlr_output) {
    this->wlr_output = wlr_output;
    this->server = server;
    wl_list_init(&workspaces);

    // create layers
    layers.background = wlr_scene_tree_create(server->layers.background);
    layers.bottom = wlr_scene_tree_create(server->layers.bottom);
    layers.top = wlr_scene_tree_create(server->layers.top);
    layers.overlay = wlr_scene_tree_create(server->layers.overlay);

    // create workspaces
    for (int i = 0; i != 9; ++i)
        new_workspace();
    set_workspace(0);

    // point output data to this
    this->wlr_output->data = this;

    // send arrange
    server->arrange();
    update_position();

    // frame
    frame.notify = [](struct wl_listener *listener, void *data) {
        // called once per frame
        struct Output *output = wl_container_of(listener, output, frame);
        struct wlr_scene *scene = output->server->scene;

        struct wlr_scene_output *scene_output =
            wlr_scene_get_scene_output(scene, output->wlr_output);

        // render scene
        wlr_scene_output_commit(scene_output, NULL);

        // get frame time
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        wlr_scene_output_send_frame_done(scene_output, &now);

        // output->arrange_layers();
    };
    wl_signal_add(&wlr_output->events.frame, &frame);

    // request_state
    request_state.notify = [](struct wl_listener *listener, void *data) {
        struct Output *output =
            wl_container_of(listener, output, request_state);

        const struct wlr_output_event_request_state *event =
            static_cast<wlr_output_event_request_state *>(data);

        wlr_output_commit_state(output->wlr_output, event->state);
        output->arrange_layers();
    };
    wl_signal_add(&wlr_output->events.request_state, &request_state);

    // destroy
    destroy.notify = [](struct wl_listener *listener, void *data) {
        struct Output *output = wl_container_of(listener, output, destroy);
        delete output;
    };
    wl_signal_add(&wlr_output->events.destroy, &destroy);
}

Output::~Output() {
    Workspace *workspace, *tmp;
    wl_list_for_each_safe(workspace, tmp, &workspaces, link) delete workspace;

    wl_list_remove(&frame.link);
    wl_list_remove(&request_state.link);
    wl_list_remove(&destroy.link);
    wl_list_remove(&link);
}

void Output::arrange_layers() {
    struct wlr_box usable = {0};
    wlr_output_effective_resolution(wlr_output, &usable.width, &usable.height);
    const struct wlr_box full_area = usable;

    // exclusive surfaces
    arrange_layer_surface(&full_area, &usable, layers.overlay, true);
    arrange_layer_surface(&full_area, &usable, layers.top, true);
    arrange_layer_surface(&full_area, &usable, layers.bottom, true);
    arrange_layer_surface(&full_area, &usable, layers.background, true);

    // non-exclusive surfaces
    arrange_layer_surface(&full_area, &usable, layers.overlay, false);
    arrange_layer_surface(&full_area, &usable, layers.top, false);
    arrange_layer_surface(&full_area, &usable, layers.bottom, false);
    arrange_layer_surface(&full_area, &usable, layers.background, false);

    // check if usable area changed
    if (memcmp(&usable, &usable_area, sizeof(struct wlr_box)) != 0)
        usable_area = usable;

    // handle keyboard interactive layers
    LayerSurface *topmost = nullptr;
    struct wlr_scene_tree *layers_above_shell[] = {layers.overlay, layers.top};

    for (auto layer : layers_above_shell) {
        struct wlr_scene_node *node;
        wl_list_for_each_reverse(node, &layer->children, link) {
            LayerSurface *surface = static_cast<LayerSurface *>(node->data);
            if (surface &&
                surface->wlr_layer_surface->current.keyboard_interactive ==
                    ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE &&
                surface->wlr_layer_surface->surface->mapped) {
                topmost = surface;
                break;
            }
        }
        if (topmost)
            break;
    }

    // TODO update keyboard focus
}

// arrange a surface layer
void Output::arrange_layer_surface(const struct wlr_box *full_area,
                                   struct wlr_box *usable_area,
                                   struct wlr_scene_tree *tree,
                                   bool exclusive) {
    struct wlr_scene_node *node;
    wl_list_for_each(node, &tree->children, link) {
        LayerSurface *surface = (LayerSurface *)node->data;
        if (!surface || !surface->wlr_layer_surface->initialized)
            continue;

        if ((surface->wlr_layer_surface->current.exclusive_zone > 0) !=
            exclusive)
            continue;

        wlr_scene_layer_surface_v1_configure(surface->scene_layer_surface,
                                             full_area, usable_area);
    }
}

// get a layer shell layer
struct wlr_scene_tree *
Output::shell_layer(enum zwlr_layer_shell_v1_layer layer) {
    switch (layer) {
    case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
        return layers.background;
    case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
        return layers.bottom;
    case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
        return layers.top;
    case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
        return layers.overlay;
    default:
        return layers.background;
    }
}

// create a new workspace for this output
struct Workspace *Output::new_workspace() {
    struct Workspace *workspace = new Workspace(this, max_workspace++);

    wl_list_insert(&workspaces, &workspace->link);
    return workspace;
}

// get the workspace numbered n
struct Workspace *Output::get_workspace(uint32_t n) {
    Workspace *workspace, *tmp;
    wl_list_for_each_safe(workspace, tmp, &workspaces,
                          link) if (workspace->num == n) return workspace;

    return nullptr;
}

// get the currently focused workspace
struct Workspace *Output::get_active() {
    if (wl_list_empty(&workspaces))
        return nullptr;

    Workspace *active = wl_container_of(workspaces.next, active, link);
    return active;
}

// change the focused workspace to workspace n
bool Output::set_workspace(uint32_t n) {
    Workspace *requested = get_workspace(n);

    // workspace does not exist
    if (requested == nullptr)
        return false;

    // hide workspace we are moving from
    Workspace *previous = get_active();
    if (previous)
        previous->set_hidden(true);

    // set new workspace to the active one
    wl_list_remove(&requested->link);
    wl_list_insert(&workspaces, &requested->link);

    // unhide active workspace and focus it
    requested->set_hidden(false);
    requested->focus();

    return true;
}

// update layout geometry
void Output::update_position() {
    wlr_output_layout_get_box(server->output_manager->layout, wlr_output,
                              &layout_geometry);
}
