#include "Server.h"

Output::Output(struct Server *server, struct wlr_output *wlr_output) {
    this->wlr_output = wlr_output;
    this->server = server;
    wl_list_init(&workspaces);

    // Create workspaces
    for (int i = 0; i != 8; ++i) {
        Workspace *w = new_workspace();
        w->set_hidden(true);
    }
    active_workspace = new_workspace();
    active_workspace->focus();

    /* Sets up a listener for the frame event. */
    frame.notify = [](struct wl_listener *listener, void *data) {
        /* This function is called every time an output is ready to display a
         * frame, generally at the output's refresh rate (e.g. 60Hz). */
        struct Output *output = wl_container_of(listener, output, frame);
        struct wlr_scene *scene = output->server->scene;

        struct wlr_scene_output *scene_output =
            wlr_scene_get_scene_output(scene, output->wlr_output);

        /* Render the scene if needed and commit the output */
        wlr_scene_output_commit(scene_output, NULL);

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        wlr_scene_output_send_frame_done(scene_output, &now);
    };
    wl_signal_add(&wlr_output->events.frame, &frame);

    /* Sets up a listener for the state request event. */
    request_state.notify = [](struct wl_listener *listener, void *data) {
        /* This function is called when the backend requests a new state for
         * the output. For example, Wayland and X11 backends request a new mode
         * when the output window is resized. */
        struct Output *output =
            wl_container_of(listener, output, request_state);
        const struct wlr_output_event_request_state *event =
            (wlr_output_event_request_state *)data;
        wlr_output_commit_state(output->wlr_output, event->state);
    };
    wl_signal_add(&wlr_output->events.request_state, &request_state);

    /* Sets up a listener for the destroy event. */
    destroy.notify = [](struct wl_listener *listener, void *data) {
        struct Output *output = wl_container_of(listener, output, destroy);
        delete output;
    };
    wl_signal_add(&wlr_output->events.destroy, &destroy);

    wl_list_insert(&server->outputs, &link);

    /* Adds this to the output layout. The add_auto function arranges outputs
     * from left-to-right in the order they appear. A more sophisticated
     * compositor would let the user configure the arrangement of outputs in the
     * layout.
     *
     * The output layout utility automatically adds a wl_output global to the
     * display, which Wayland clients can see to find out information about the
     * output (such as DPI, scale factor, manufacturer, etc).
     */
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

struct Workspace *Output::new_workspace() {
    struct Workspace *workspace = new Workspace(this);
    wl_list_insert(&workspaces, &workspace->link);
    return workspace;
}

struct Workspace *Output::get_workspace(uint32_t n) {
    Workspace *workspace, *tmp;
    uint32_t index = 0;
    wl_list_for_each_safe(workspace, tmp, &workspaces, link) {
        if (index == n)
            return workspace;
        else
            ++index;
    }

    return nullptr;
}

bool Output::set_workspace(uint32_t n) {
    Workspace *requested = get_workspace(n);

    if (requested == nullptr)
        return false;

    active_workspace->set_hidden(true);
    active_workspace = requested;
    active_workspace->set_hidden(false);
    active_workspace->focus();

    return true;
}
