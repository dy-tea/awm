#include "Server.h"

Popup::Popup(struct wlr_xdg_popup *xdg_popup) {
    this->xdg_popup = xdg_popup;

    // add popup to scene graph
    struct wlr_xdg_surface *parent =
        wlr_xdg_surface_try_from_wlr_surface(xdg_popup->parent);
    assert(parent != NULL);
    struct wlr_scene_tree *parent_tree = (wlr_scene_tree *)parent->data;
    xdg_popup->base->data =
        wlr_scene_xdg_surface_create(parent_tree, xdg_popup->base);

    // xdg_popup_commit
    commit.notify = [](struct wl_listener *listener, void *data) {
        struct Popup *popup = wl_container_of(listener, popup, commit);

        if (popup->xdg_popup->base->initial_commit) {
            // TODO check if outside of output
            wlr_xdg_surface_schedule_configure(popup->xdg_popup->base);
        }
    };
    wl_signal_add(&xdg_popup->base->surface->events.commit, &commit);

    // xdg_popup_destroy
    destroy.notify = [](struct wl_listener *listener, void *data) {
        struct Popup *popup = wl_container_of(listener, popup, destroy);
        delete popup;
    };
    wl_signal_add(&xdg_popup->events.destroy, &destroy);
}

Popup::~Popup() {
    wl_list_remove(&commit.link);
    wl_list_remove(&destroy.link);
}
