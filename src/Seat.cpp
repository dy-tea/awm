#include "Server.h"

Seat::Seat(Server *server) : server(server) {
    wlr_seat = wlr_seat_create(server->display, "seat0");

    // new_input
    new_input.notify = [](wl_listener *listener, void *data) {
        // create input device based on type
        Seat *seat = wl_container_of(listener, seat, new_input);
        Server *server = seat->server;

        // handle device type
        switch (auto *device = static_cast<wlr_input_device *>(data);
                device->type) {
        case WLR_INPUT_DEVICE_KEYBOARD: {
            // create keyboard
            Keyboard *keyboard =
                new Keyboard(server, wlr_keyboard_from_input_device(device));

            // set destroy listener
            wl_signal_add(&device->events.destroy, &keyboard->destroy);
            break;
        }
        case WLR_INPUT_DEVICE_POINTER: {
            const auto pointer = reinterpret_cast<wlr_pointer *>(device);

            // set the cursor configuration
            server->cursor->set_config(pointer);

            // attach to device
            wlr_cursor_attach_input_device(server->cursor->cursor,
                                           &pointer->base);
            break;
        }
        default:
            break;
        }

        // set input device capabilities
        uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
        if (!wl_list_empty(&server->keyboards))
            caps |= WL_SEAT_CAPABILITY_KEYBOARD;

        wlr_seat_set_capabilities(seat->wlr_seat, caps);
    };
    wl_signal_add(&server->backend->events.new_input, &new_input);

    // request_cursor
    request_cursor.notify = [](wl_listener *listener, void *data) {
        // client-provided cursor image
        Seat *seat = wl_container_of(listener, seat, request_cursor);
        const auto *event =
            static_cast<wlr_seat_pointer_request_set_cursor_event *>(data);

        // only obey focused client
        if (seat->wlr_seat->pointer_state.focused_client == event->seat_client)
            wlr_cursor_set_surface(seat->server->cursor->cursor, event->surface,
                                   event->hotspot_x, event->hotspot_y);
    };
    wl_signal_add(&wlr_seat->events.request_set_cursor, &request_cursor);

    // request_set_selection
    request_set_selection.notify = [](wl_listener *listener, void *data) {
        // user selection
        Seat *seat = wl_container_of(listener, seat, request_set_selection);

        const auto *event =
            static_cast<wlr_seat_request_set_selection_event *>(data);

        wlr_seat_set_selection(seat->wlr_seat, event->source, event->serial);
    };
    wl_signal_add(&wlr_seat->events.request_set_selection,
                  &request_set_selection);

    // request_set_primary_selection
    request_set_primary_selection.notify = [](wl_listener *listener,
                                              void *data) {
        Seat *seat =
            wl_container_of(listener, seat, request_set_primary_selection);

        const auto *event =
            static_cast<wlr_seat_request_set_primary_selection_event *>(data);

        wlr_seat_set_primary_selection(seat->wlr_seat, event->source,
                                       event->serial);
    };
    wl_signal_add(&wlr_seat->events.request_set_primary_selection,
                  &request_set_primary_selection);

    // request_start_drag
    request_start_drag.notify = [](wl_listener *listener, void *data) {
        Seat *seat = wl_container_of(listener, seat, request_start_drag);

        const auto *event =
            static_cast<wlr_seat_request_start_drag_event *>(data);

        if (wlr_seat_validate_pointer_grab_serial(seat->wlr_seat, event->origin,
                                                  event->serial))
            wlr_seat_start_pointer_drag(seat->wlr_seat, event->drag,
                                        event->serial);
        else
            wlr_data_source_destroy(event->drag->source);
    };
    wl_signal_add(&wlr_seat->events.request_start_drag, &request_start_drag);

    // start_drag
    start_drag.notify = [](wl_listener *listener, void *data) {
        Seat *seat = wl_container_of(listener, seat, start_drag);
        Server *server = seat->server;

        const auto *drag = static_cast<wlr_drag *>(data);
        if (!drag->icon)
            return;

        // create a drag icon in the scene
        drag->icon->data =
            &wlr_scene_drag_icon_create(server->layers.drag_icon, drag->icon)
                 ->node;

        // set up destroy listener
        seat->destroy_drag_icon.notify = [](wl_listener *listener,
                                            [[maybe_unused]] void *data) {
            Seat *seat = wl_container_of(listener, seat, destroy_drag_icon);
            wl_list_remove(&seat->destroy_drag_icon.link);
        };
        wl_signal_add(&drag->icon->events.destroy, &seat->destroy_drag_icon);
    };
    wl_signal_add(&wlr_seat->events.start_drag, &start_drag);

    // virtual pointer
    virtual_pointer_mgr =
        wlr_virtual_pointer_manager_v1_create(server->display);

    // new_virtual_pointer
    new_virtual_pointer.notify = [](wl_listener *listener, void *data) {
        Seat *seat = wl_container_of(listener, seat, new_virtual_pointer);
        Server *server = seat->server;

        const auto *event =
            static_cast<wlr_virtual_pointer_v1_new_pointer_event *>(data);
        wlr_virtual_pointer_v1 *pointer = event->new_pointer;
        wlr_input_device *device = &pointer->pointer.base;

        wlr_cursor_attach_input_device(server->cursor->cursor, device);
        if (event->suggested_output)
            wlr_cursor_map_input_to_output(server->cursor->cursor, device,
                                           event->suggested_output);
    };
    wl_signal_add(&virtual_pointer_mgr->events.new_virtual_pointer,
                  &new_virtual_pointer);

    // virtual keyboard
    virtual_keyboard_manager =
        wlr_virtual_keyboard_manager_v1_create(server->display);

    // new_virtual_keyboard
    new_virtual_keyboard.notify = [](wl_listener *listener, void *data) {
        Seat *seat = wl_container_of(listener, seat, new_virtual_keyboard);

        auto *virtual_keyboard = static_cast<wlr_virtual_keyboard_v1 *>(data);
        Keyboard *keyboard =
            new Keyboard(seat->server, &virtual_keyboard->keyboard);

        // set up destroy listener
        wl_signal_add(&virtual_keyboard->keyboard.base.events.destroy,
                      &keyboard->destroy);
    };
    wl_signal_add(&virtual_keyboard_manager->events.new_virtual_keyboard,
                  &new_virtual_keyboard);
}

Seat::~Seat() {
    wl_list_remove(&new_input.link);
    wl_list_remove(&request_cursor.link);
    wl_list_remove(&request_set_selection.link);
    wl_list_remove(&request_set_primary_selection.link);
    wl_list_remove(&request_start_drag.link);
    wl_list_remove(&start_drag.link);

    wl_list_remove(&new_virtual_pointer.link);
    wl_list_remove(&new_virtual_keyboard.link);
}
