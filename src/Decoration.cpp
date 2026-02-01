#include "Decoration.h"
#include "Server.h"
#include "Toplevel.h"
#include "wlr.h"

const int TITLEBAR_HEIGHT = 30;
const int BTN_SIZE = 20;
const int BTN_MARGIN = 5;

const float COL_TITLEBAR[4] = {0.2f, 0.2f, 0.2f, 1.0f};
const float COL_CLOSE[4] = {0.8f, 0.2f, 0.2f, 1.0f};
const float COL_MAX[4] = {0.2f, 0.8f, 0.2f, 1.0f};
const float COL_FULL[4] = {0.2f, 0.2f, 0.8f, 1.0f};

// creates the titlebar and all button nodes
void Decoration::create_nodes() {
    titlebar =
        wlr_scene_rect_create(scene_tree, 400, TITLEBAR_HEIGHT, COL_TITLEBAR);
    wlr_scene_node_set_position(&titlebar->node, 0, -TITLEBAR_HEIGHT);
    btn_close =
        wlr_scene_rect_create(scene_tree, BTN_SIZE, BTN_SIZE, COL_CLOSE);
    btn_max = wlr_scene_rect_create(scene_tree, BTN_SIZE, BTN_SIZE, COL_MAX);
    btn_full = wlr_scene_rect_create(scene_tree, BTN_SIZE, BTN_SIZE, COL_FULL);
}

// resize the titlebar with a new width
void Decoration::update_titlebar(int width) {
    if (width <= 0 || !titlebar)
        return;

    wlr_scene_rect_set_size(titlebar, width, TITLEBAR_HEIGHT);

    int x_pos = width - BTN_SIZE - BTN_MARGIN;
    int y_pos = -TITLEBAR_HEIGHT + BTN_MARGIN;

    wlr_scene_node_set_position(&btn_close->node, std::max(x_pos, 0), y_pos);

    x_pos -= (BTN_SIZE + BTN_MARGIN);
    if (x_pos > 0) {
        wlr_scene_node_set_position(&btn_max->node, x_pos, y_pos);
    }
    wlr_scene_node_set_enabled(&btn_max->node, x_pos > 0);

    x_pos -= (BTN_SIZE + BTN_MARGIN);
    if (x_pos > 0) {
        wlr_scene_node_set_position(&btn_full->node, x_pos, y_pos);
    }
    wlr_scene_node_set_enabled(&btn_full->node, x_pos > 0);
}

void Decoration::set_visible(bool visible) {
    this->visible = visible;
    wlr_scene_node_set_enabled(&scene_tree->node, visible);
}

// returns the button corresponding to a given scene node
DecorationPart Decoration::get_part_from_node(struct wlr_scene_node *node) {
    if (node == &titlebar->node)
        return DecorationPart::TITLEBAR;
    if (node == &btn_close->node)
        return DecorationPart::CLOSE_BUTTON;
    if (node == &btn_max->node)
        return DecorationPart::MAXIMIZE_BUTTON;
    if (node == &btn_full->node)
        return DecorationPart::FULLSCREEN_BUTTON;
    return DecorationPart::NONE;
}

Decoration::Decoration(Server *server,
                       wlr_xdg_toplevel_decoration_v1 *decoration)
    : server(server),
      toplevel(static_cast<Toplevel *>(decoration->toplevel->base->data)),
      decoration(decoration),
      scene_tree(wlr_scene_tree_create(toplevel->scene_tree)) {

    wlr_scene_node_raise_to_top(&scene_tree->node);
    wl_list_insert(&server->decorations, &link);
    toplevel->decoration = this;
    scene_tree->node.data = this;

    commit.notify = nullptr;

    // mode
    mode.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        Decoration *deco = wl_container_of(listener, deco, mode);
        wlr_xdg_toplevel_decoration_v1_mode client_mode =
            deco->decoration->requested_mode;

        deco->client_mode =
            (client_mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_NONE)
                ? WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE
                : client_mode;

        if (deco->decoration->toplevel->base->initialized) {
            wlr_xdg_toplevel_decoration_v1_set_mode(deco->decoration,
                                                    client_mode);
        } else if (!deco->commit.notify) {
            // listen to surface commits to update titlebar size
            deco->commit.notify = [](wl_listener *listener,
                                     [[maybe_unused]] void *data) {
                Decoration *deco = wl_container_of(listener, deco, commit);
                if (deco->decoration->toplevel->base->initial_commit) {
                    wlr_xdg_toplevel_decoration_v1_set_mode(deco->decoration,
                                                            deco->client_mode);
                    wl_list_remove(&deco->commit.link);
                    deco->commit.notify = nullptr;
                }
                if (deco->toplevel->decoration_mode ==
                    WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE) {
                    if (!deco->titlebar)
                        deco->create_nodes();
                    deco->update_titlebar(deco->toplevel->geometry.width);
                }
            };
            wl_signal_add(
                &deco->decoration->toplevel->base->surface->events.commit,
                &deco->commit);
        }
        deco->toplevel->set_decoration_mode(client_mode);
    };
    wl_signal_add(&decoration->events.request_mode, &mode);

    // destroy
    destroy.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        Decoration *deco = wl_container_of(listener, deco, destroy);
        delete deco;
    };
    wl_signal_add(&decoration->events.destroy, &destroy);
}

Decoration::~Decoration() {
    if (toplevel)
        toplevel->decoration = nullptr;

    wl_list_remove(&mode.link);
    wl_list_remove(&destroy.link);
    wl_list_remove(&link);

    wlr_scene_node_destroy(&btn_close->node);
    wlr_scene_node_destroy(&btn_max->node);
    wlr_scene_node_destroy(&btn_full->node);
    wlr_scene_node_destroy(&scene_tree->node);

    if (commit.notify) {
        wl_list_remove(&commit.link);
        commit.notify = nullptr;
    }
}
