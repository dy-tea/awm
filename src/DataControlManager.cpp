#include "Server.h"

DataControlManager::DataControlManager(struct wl_display *display) {
    // create the manager
    wlr_data_control_manager = wlr_data_control_manager_v1_create(display);

    // destroy
    destroy.notify = [](struct wl_listener *listener, void *data) {
        DataControlManager *manager =
            wl_container_of(listener, manager, destroy);
        delete manager;
    };
    wl_signal_add(&wlr_data_control_manager->events.destroy, &destroy);

    // new_device
    new_device.notify = [](struct wl_listener *listener, void *data) {
        DataControlManager *manager =
            wl_container_of(listener, manager, new_device);

        wlr_data_control_device_v1 *device = (wlr_data_control_device_v1 *)data;
        wl_list_insert(&manager->wlr_data_control_manager->devices,
                       &device->link);
    };
    wl_signal_add(&wlr_data_control_manager->events.new_device, &new_device);
}

DataControlManager::~DataControlManager() {
    wl_list_remove(&destroy.link);
    wl_list_remove(&new_device.link);
}
