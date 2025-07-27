#include "IdleInhibitor.h"
#include "Server.h"

IdleInhibitor::IdleInhibitor(wlr_idle_inhibitor_v1 *idle_inhibitor,
                             Server *server)
    : idle_inhibitor(idle_inhibitor), server(server) {
    // destroy listener
    destroy.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        IdleInhibitor *inhibitor =
            wl_container_of(listener, inhibitor, destroy);
        delete inhibitor;
    };
    wl_signal_add(&idle_inhibitor->events.destroy, &destroy);

    // call update
    server->update_idle_inhibitors(nullptr);
}

IdleInhibitor::~IdleInhibitor() {
    server->update_idle_inhibitors(
        wlr_surface_get_root_surface(idle_inhibitor->surface));
    wl_list_remove(&destroy.link);
}
