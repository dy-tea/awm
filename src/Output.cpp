#include "Server.h"

Output::Output(struct Server *server, struct wlr_output *wlr_output) {
    this->wlr_output = wlr_output;
    this->server = server;
    wl_list_init(&workspaces);

    // create workspaces
    for (int i = 0; i != 9; ++i)
        new_workspace();
    set_workspace(0);

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
    };
    wl_signal_add(&wlr_output->events.frame, &frame);

    // request_state
    request_state.notify = [](struct wl_listener *listener, void *data) {
        struct Output *output =
            wl_container_of(listener, output, request_state);

        const struct wlr_output_event_request_state *event =
            (wlr_output_event_request_state *)data;

        wlr_output_commit_state(output->wlr_output, event->state);
    };
    wl_signal_add(&wlr_output->events.request_state, &request_state);

    // destroy
    destroy.notify = [](struct wl_listener *listener, void *data) {
        struct Output *output = wl_container_of(listener, output, destroy);
        delete output;
    };
    wl_signal_add(&wlr_output->events.destroy, &destroy);

    // add to output layout TODO configurable output laout
    struct wlr_output_layout_output *l_output =
        wlr_output_layout_add_auto(server->output_layout, wlr_output);
    struct wlr_scene_output *scene_output =
        wlr_scene_output_create(server->scene, wlr_output);
    wlr_scene_output_layout_add_output(server->scene_layout, l_output,
                                       scene_output);
}

Output::~Output() {
    Workspace *workspace, *tmp;
    wl_list_for_each_safe(workspace, tmp, &workspaces, link) delete workspace;

    wl_list_remove(&frame.link);
    wl_list_remove(&request_state.link);
    wl_list_remove(&destroy.link);
    wl_list_remove(&link);
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
