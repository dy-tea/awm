#include "Server.h"

Popup::Popup(struct wlr_xdg_popup *xdg_popup) {
    this->xdg_popup = xdg_popup;

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
	commit.notify = [](struct wl_listener *listener, void *data) {
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
	wl_signal_add(&xdg_popup->base->surface->events.commit, &commit);

	// xdg_popup_destroy
	destroy.notify = [](struct wl_listener *listener, void *data) {
		/* Called when the xdg_popup is destroyed. */
		struct Popup *popup = wl_container_of(listener, popup, destroy);
		delete popup;
	};
	wl_signal_add(&xdg_popup->events.destroy, &destroy);
}

Popup::~Popup() {
    wl_list_remove(&commit.link);
	wl_list_remove(&destroy.link);
}
