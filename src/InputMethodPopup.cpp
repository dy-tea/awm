#include "InputMethodPopup.h"
#include "InputRelay.h"
#include "Server.h"

// based on labwc implementation

InputMethodPopup::InputMethodPopup(InputRelay *relay,
                                   wlr_input_popup_surface_v2 *popup_surface)
    : relay(relay), popup_surface(popup_surface),
      scene_tree(wlr_scene_subsurface_tree_create(relay->popup_tree,
                                                  popup_surface->surface)) {

    commit.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        InputMethodPopup *popup = wl_container_of(listener, popup, commit);
        popup->update_position();
    };
    wl_signal_add(&popup_surface->surface->events.commit, &commit);

    destroy.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        InputMethodPopup *popup = wl_container_of(listener, popup, destroy);
        delete popup;
    };
    wl_signal_add(&popup_surface->events.destroy, &destroy);
}

InputMethodPopup::~InputMethodPopup() {
    wlr_scene_node_destroy(&scene_tree->node);
    wl_list_remove(&destroy.link);
    wl_list_remove(&commit.link);
    wl_list_remove(&link);
}

void InputMethodPopup::update_position() {
    Server *server = relay->seat->server;
    TextInput *text_input = relay->active_text_input;

    if (!text_input || !relay->focused_surface ||
        !popup_surface->surface->mapped)
        return;

    wlr_box cursor_geo;
    wlr_xdg_surface *xdg_surface =
        wlr_xdg_surface_try_from_wlr_surface(relay->focused_surface);
    wlr_layer_surface_v1 *layer_surface =
        wlr_layer_surface_v1_try_from_wlr_surface(relay->focused_surface);

    if ((text_input->wlr_text_input->current.features &
         WLR_TEXT_INPUT_V3_FEATURE_CURSOR_RECTANGLE) &&
        (xdg_surface || layer_surface)) {
        cursor_geo = text_input->wlr_text_input->current.cursor_rectangle;

        wlr_scene_tree *tree =
            static_cast<wlr_scene_tree *>(relay->focused_surface->data);
        int lx, ly;
        wlr_scene_node_coords(&tree->node, &lx, &ly);
        cursor_geo.x += lx;
        cursor_geo.y += ly;

        if (xdg_surface) {
            cursor_geo.x -= xdg_surface->geometry.x;
            cursor_geo.y -= xdg_surface->geometry.y;
        }
    } else
        cursor_geo = {0, 0, 0, 0};

    Output *output =
        server->output_manager->output_at(cursor_geo.x, cursor_geo.y);
    if (!output || !output->enabled)
        return;
    wlr_box output_geo = output->usable_area;

    wlr_xdg_positioner_rules rules = {
        cursor_geo,
        XDG_POSITIONER_ANCHOR_BOTTOM_LEFT,
        XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT,
        XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_NONE,
        false,
        false,
        XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y |
            XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X,
        {popup_surface->surface->current.width,
         popup_surface->surface->current.height},
        {0, 0},
        {0, 0}};

    wlr_box popup_geo;
    wlr_xdg_positioner_rules_get_geometry(&rules, &popup_geo);
    wlr_xdg_positioner_rules_unconstrain_box(&rules, &output_geo, &popup_geo);

    wlr_scene_node_set_position(&scene_tree->node, popup_geo.x, popup_geo.y);
    wlr_scene_node_raise_to_top(&scene_tree->node);

    wlr_box input_rect = {cursor_geo.x - popup_geo.x,
                          cursor_geo.y - popup_geo.y, cursor_geo.width,
                          cursor_geo.height};
    wlr_input_popup_surface_v2_send_text_input_rectangle(popup_surface,
                                                         &input_rect);
}
