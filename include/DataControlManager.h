#include "wlr.h"

struct DataControlManager {
    struct wlr_data_control_manager_v1 *wlr_data_control_manager;

    struct wl_listener destroy;
    struct wl_listener new_device;

    DataControlManager(struct wl_display *display);
    ~DataControlManager();
};
