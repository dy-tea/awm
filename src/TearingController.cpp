#include "TearingController.h"
#include "Server.h"

TearingController::TearingController(Server *server,
                                     wlr_tearing_control_v1 *tearing_control)
    : server(server), tearing_control(tearing_control) {
    wl_list_insert(&server->tearing_controllers, &link);

    // set_hint
    set_hint.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        TearingController *controller =
            wl_container_of(listener, controller, set_hint);

        if (Toplevel *toplevel = controller->server->get_toplevel(
                controller->tearing_control->surface))
            toplevel->tearing_hint = controller->tearing_control->current;
    };
    wl_signal_add(&tearing_control->events.set_hint, &set_hint);

    // destroy
    destroy.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        TearingController *controller =
            wl_container_of(listener, controller, destroy);
        delete controller;
    };
    wl_signal_add(&tearing_control->events.destroy, &destroy);
}

TearingController::~TearingController() {
    wl_list_remove(&link);
    wl_list_remove(&set_hint.link);
    wl_list_remove(&destroy.link);
}
