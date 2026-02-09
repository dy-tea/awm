#include "BSPTree.h"
#include "Server.h"
#include "Toplevel.h"
#include "Transaction.h"
#include "Workspace.h"
#include <algorithm>
#include <functional>

BSPNode *BSPNode::find_toplevel(Toplevel *tl) {
    if (is_leaf())
        return toplevel == tl ? this : nullptr;

    if (first_child)
        if (BSPNode *found = first_child->find_toplevel(tl))
            return found;

    if (second_child)
        if (BSPNode *found = second_child->find_toplevel(tl))
            return found;

    return nullptr;
}

bool BSPNode::remove_toplevel(Toplevel *tl) {
    if (is_leaf()) {
        if (toplevel == tl) {
            toplevel = nullptr;
            return true;
        }
        return false;
    }

    bool removed = false;
    if (first_child && first_child->remove_toplevel(tl))
        removed = true;
    if (second_child && second_child->remove_toplevel(tl))
        removed = true;

    return removed;
}

void BSPNode::get_toplevels(std::vector<Toplevel *> &toplevels) const {
    if (is_leaf()) {
        if (toplevel)
            toplevels.push_back(toplevel);
        return;
    }

    if (first_child)
        first_child->get_toplevels(toplevels);
    if (second_child)
        second_child->get_toplevels(toplevels);
}

int BSPNode::count_leaves() const {
    if (is_leaf())
        return toplevel ? 1 : 0;

    int count = 0;
    if (first_child)
        count += first_child->count_leaves();
    if (second_child)
        count += second_child->count_leaves();

    return count;
}

BSPNode *BSPNode::find_insertion_point() {
    if (is_leaf())
        return this;

    int first_count = first_child ? first_child->count_leaves() : 0;
    int second_count = second_child ? second_child->count_leaves() : 0;

    if (first_count <= second_count && first_child)
        return first_child->find_insertion_point();
    else if (second_child)
        return second_child->find_insertion_point();
    else if (first_child)
        return first_child->find_insertion_point();

    return this;
}

void BSPTree::insert(Toplevel *toplevel) {
    if (!toplevel)
        return;

    if (!root) {
        root = std::make_unique<BSPNode>(toplevel);
        return;
    }

    BSPNode *insertion_point = root->find_insertion_point();
    if (!insertion_point || !insertion_point->is_leaf())
        return;

    Toplevel *existing = insertion_point->toplevel;
    insertion_point->toplevel = nullptr;

    insertion_point->split = determine_split_type(insertion_point);
    insertion_point->ratio = 0.5f;

    insertion_point->first_child = std::make_unique<BSPNode>(existing);
    insertion_point->first_child->parent = insertion_point;

    insertion_point->second_child = std::make_unique<BSPNode>(toplevel);
    insertion_point->second_child->parent = insertion_point;
}

void BSPTree::insert_at(Toplevel *toplevel, Toplevel *target) {
    if (!toplevel)
        return;

    if (!root) {
        root = std::make_unique<BSPNode>(toplevel);
        return;
    }

    BSPNode *target_node = target ? find_node(target) : nullptr;

    if (!target_node || !target_node->is_leaf()) {
        insert(toplevel);
        return;
    }

    Toplevel *existing = target_node->toplevel;
    target_node->toplevel = nullptr;

    target_node->split = determine_split_type(target_node);
    target_node->ratio = 0.5f;

    target_node->first_child = std::make_unique<BSPNode>(existing);
    target_node->first_child->parent = target_node;

    target_node->second_child = std::make_unique<BSPNode>(toplevel);
    target_node->second_child->parent = target_node;
}

void BSPTree::remove(Toplevel *toplevel) {
    if (!root || !toplevel)
        return;

    BSPNode *node = find_node(toplevel);
    if (!node || !node->is_leaf())
        return;

    node->toplevel = nullptr;

    BSPNode *parent = node->parent;

    if (!parent) {
        root.reset();
        return;
    }

    BSPNode *sibling = nullptr;
    if (parent->first_child.get() == node)
        sibling = parent->second_child.get();
    else
        sibling = parent->first_child.get();

    if (!sibling)
        return;

    if (!parent->parent) {
        root->split = sibling->split;
        root->ratio = sibling->ratio;
        root->toplevel = sibling->toplevel;
        root->first_child = std::move(sibling->first_child);
        root->second_child = std::move(sibling->second_child);

        if (root->first_child)
            root->first_child->parent = root.get();
        if (root->second_child)
            root->second_child->parent = root.get();

        return;
    }

    BSPNode *grandparent = parent->parent;
    if (grandparent->first_child.get() == parent) {
        std::unique_ptr<BSPNode> sibling_ptr;
        if (parent->first_child.get() == node)
            sibling_ptr = std::move(parent->second_child);
        else
            sibling_ptr = std::move(parent->first_child);

        sibling_ptr->parent = grandparent;
        grandparent->first_child = std::move(sibling_ptr);
    } else {
        std::unique_ptr<BSPNode> sibling_ptr;
        if (parent->first_child.get() == node)
            sibling_ptr = std::move(parent->second_child);
        else
            sibling_ptr = std::move(parent->first_child);

        sibling_ptr->parent = grandparent;
        grandparent->second_child = std::move(sibling_ptr);
    }
}

void BSPTree::apply_layout(const wlr_box &bounds, bool use_transaction) {
    if (!root)
        return;

    if (use_transaction)
        workspace->output->server->transaction_manager->begin();

    calculate_layout(root.get(), bounds);
    apply_geometries(root.get(), !use_transaction);

    if (use_transaction)
        workspace->output->server->transaction_manager->commit();
}

BSPNode *BSPTree::find_node(Toplevel *toplevel) {
    if (!root)
        return nullptr;
    return root->find_toplevel(toplevel);
}

bool BSPTree::get_toplevel_geometry(Toplevel *toplevel, const wlr_box &bounds,
                                    wlr_box &out_geometry) {
    if (!root || !toplevel)
        return false;

    calculate_layout(root.get(), bounds);

    BSPNode *node = find_node(toplevel);
    if (!node || !node->is_leaf())
        return false;

    out_geometry = node->geometry;
    return true;
}

void BSPTree::adjust_ratio(BSPNode *node, float new_ratio) {
    if (!node || node->is_leaf())
        return;

    node->ratio = std::max(0.1f, std::min(0.9f, new_ratio));
}

void BSPTree::handle_resize(Toplevel *toplevel, const wlr_box &new_geo) {
    BSPNode *node = find_node(toplevel);
    if (!node || !node->parent)
        return;

    BSPNode *parent = node->parent;
    float new_ratio = calculate_ratio_from_resize(parent, node, new_geo);

    if (new_ratio >= 0.0f)
        adjust_ratio(parent, new_ratio);
}

void BSPTree::rebuild(std::vector<Toplevel *> toplevels) {
    clear();

    for (Toplevel *tl : toplevels)
        if (tl && !tl->fullscreen() && !tl->maximized())
            insert(tl);
}

void BSPTree::rebuild_grid(std::vector<Toplevel *> toplevels) {
    clear();

    if (toplevels.empty())
        return;

    // filter out fullscreen and maximized toplevels
    std::vector<Toplevel *> tiled;
    for (Toplevel *tl : toplevels)
        if (tl && !tl->fullscreen() && !tl->maximized())
            tiled.push_back(tl);

    if (tiled.empty())
        return;

    int count = tiled.size();

    // calculate grid dimensions
    int rows = std::round(std::sqrt(count));
    int cols = (count + rows - 1) / rows;

    // bsp tree for grid
    root = std::make_unique<BSPNode>();

    if (count == 1) {
        root->toplevel = tiled[0];
        return;
    }

    // recursive helper to build grid tree
    std::function<void(BSPNode *, int, int, int, int, int)> build_grid_node;
    build_grid_node = [&](BSPNode *node, int start_idx, int end_idx,
                          int start_row, int end_row, int depth) {
        int num_windows = end_idx - start_idx;
        int num_rows = end_row - start_row;

        if (num_windows == 0)
            return;

        if (num_windows == 1) {
            // leaf node
            node->toplevel = tiled[start_idx];
            node->split = SplitType::NONE;
            return;
        }

        // split horizontally into rows
        if (num_rows > 1) {
            int mid_row = start_row + num_rows / 2;
            int windows_in_first = mid_row * cols - start_row * cols;

            // clamp to window count
            if (start_idx + windows_in_first > end_idx)
                windows_in_first = (end_idx - start_idx + 1) / 2;

            int mid_idx = start_idx + windows_in_first;

            node->split = SplitType::HORIZONTAL;
            node->ratio = static_cast<float>(windows_in_first) / num_windows;

            node->first_child = std::make_unique<BSPNode>();
            node->first_child->parent = node;
            build_grid_node(node->first_child.get(), start_idx, mid_idx,
                            start_row, mid_row, depth + 1);

            node->second_child = std::make_unique<BSPNode>();
            node->second_child->parent = node;
            build_grid_node(node->second_child.get(), mid_idx, end_idx, mid_row,
                            end_row, depth + 1);
        } else {
            // single row
            int mid_idx = start_idx + num_windows / 2;

            node->split = SplitType::VERTICAL;
            node->ratio = 0.5f;

            node->first_child = std::make_unique<BSPNode>();
            node->first_child->parent = node;
            build_grid_node(node->first_child.get(), start_idx, mid_idx,
                            start_row, end_row, depth + 1);

            node->second_child = std::make_unique<BSPNode>();
            node->second_child->parent = node;
            build_grid_node(node->second_child.get(), mid_idx, end_idx,
                            start_row, end_row, depth + 1);
        }
    };

    build_grid_node(root.get(), 0, count, 0, rows, 0);
}

void BSPTree::rebuild_dwindle(std::vector<Toplevel *> toplevels) {
    clear();

    if (toplevels.empty())
        return;

    // filter out fullscreen and maximized toplevels
    std::vector<Toplevel *> tiled;
    for (Toplevel *tl : toplevels)
        if (tl && !tl->fullscreen() && !tl->maximized())
            tiled.push_back(tl);

    if (tiled.empty())
        return;

    int count = tiled.size();

    // build dwindle tree
    root = std::make_unique<BSPNode>();

    if (count == 1) {
        root->toplevel = tiled[0];
        return;
    }

    // recursive helper to build dwindle tree
    std::function<void(BSPNode *, int, int, bool)> build_dwindle_node;
    build_dwindle_node = [&](BSPNode *node, int start_idx, int end_idx,
                             bool is_horizontal) {
        int num_windows = end_idx - start_idx;

        if (num_windows == 0)
            return;

        if (num_windows == 1) {
            // leaf node
            node->toplevel = tiled[start_idx];
            node->split = SplitType::NONE;
            return;
        }

        // alternate split direction
        node->split =
            is_horizontal ? SplitType::HORIZONTAL : SplitType::VERTICAL;
        node->ratio = 0.5f;

        // 1st child
        node->first_child = std::make_unique<BSPNode>();
        node->first_child->parent = node;
        node->first_child->toplevel = tiled[start_idx];
        node->first_child->split = SplitType::NONE;

        // 2nd child
        node->second_child = std::make_unique<BSPNode>();
        node->second_child->parent = node;

        if (num_windows == 2) {
            node->second_child->toplevel = tiled[start_idx + 1];
            node->second_child->split = SplitType::NONE;
        } else {
            // recursively split the rest with alternating direction
            build_dwindle_node(node->second_child.get(), start_idx + 1, end_idx,
                               !is_horizontal);
        }
    };

    build_dwindle_node(root.get(), 0, count, true);
}

void BSPTree::insert_at_dwindle(Toplevel *toplevel, Toplevel *target) {
    if (!toplevel)
        return;

    if (!root) {
        root = std::make_unique<BSPNode>(toplevel);
        return;
    }

    BSPNode *target_node = target ? find_node(target) : nullptr;

    if (!target_node || !target_node->is_leaf()) {
        // No valid target, add to tree using standard dwindle rebuild
        std::vector<Toplevel *> tls;
        root->get_toplevels(tls);
        tls.push_back(toplevel);
        rebuild_dwindle(tls);
        return;
    }

    // Split the target node horizontally (active on left, new on right)
    Toplevel *existing = target_node->toplevel;
    target_node->toplevel = nullptr;

    target_node->split = SplitType::HORIZONTAL;
    target_node->ratio = 0.5f;

    // Active window goes on the left (first child)
    target_node->first_child = std::make_unique<BSPNode>(existing);
    target_node->first_child->parent = target_node;

    // New window goes on the right (second child)
    target_node->second_child = std::make_unique<BSPNode>(toplevel);
    target_node->second_child->parent = target_node;
}

void BSPTree::clear() { root.reset(); }

void BSPTree::calculate_layout(BSPNode *node, const wlr_box &bounds) {
    if (!node)
        return;

    node->geometry = bounds;

    if (node->is_leaf())
        return;

    wlr_box first_bounds = bounds;
    wlr_box second_bounds = bounds;

    if (node->split == SplitType::HORIZONTAL) {
        int split_x = bounds.x + static_cast<int>(bounds.width * node->ratio);

        first_bounds.width = split_x - bounds.x;
        second_bounds.x = split_x;
        second_bounds.width = bounds.width - first_bounds.width;
    } else if (node->split == SplitType::VERTICAL) {
        int split_y = bounds.y + static_cast<int>(bounds.height * node->ratio);

        first_bounds.height = split_y - bounds.y;
        second_bounds.y = split_y;
        second_bounds.height = bounds.height - first_bounds.height;
    }

    if (node->first_child)
        calculate_layout(node->first_child.get(), first_bounds);
    if (node->second_child)
        calculate_layout(node->second_child.get(), second_bounds);
}

void BSPTree::apply_geometries(BSPNode *node, bool immediate) {
    if (!node)
        return;

    if (node->is_leaf() && node->toplevel) {
        if (!node->toplevel->maximized() && !node->toplevel->fullscreen()) {
            if (immediate) {
                Toplevel *tl = node->toplevel;
                wlr_box &geo = node->geometry;

                tl->geometry = geo;

                int x = geo.x;
                int y = geo.y;

                // remove decoration height from window height
                int width = geo.width;
                int height = geo.height;

                // apply decoration offset if decoration is visible
                if (tl->decoration &&
                    tl->decoration_mode ==
                        WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE &&
                    tl->decoration->visible) {
                    int deco_height = 30;
                    y += deco_height;
                    height -= deco_height;
                }

                wlr_scene_node_set_position(&tl->scene_tree->node, x, y);

#ifdef XWAYLAND
                if (tl->xdg_toplevel) {
#endif
                    wlr_xdg_toplevel_set_size(tl->xdg_toplevel, width, height);
                    wlr_xdg_surface_schedule_configure(tl->xdg_toplevel->base);
#ifdef XWAYLAND
                } else if (tl->xwayland_surface) {
                    wlr_xwayland_surface_configure(tl->xwayland_surface, geo.x,
                                                   geo.y, width, height);
                }
#endif

                if (tl->decoration)
                    tl->decoration->update_titlebar(geo.width);
            } else {
                node->toplevel->set_position_size(node->geometry);
            }
        }
        return;
    }

    if (node->first_child)
        apply_geometries(node->first_child.get(), immediate);
    if (node->second_child)
        apply_geometries(node->second_child.get(), immediate);
}

void BSPTree::dump_tree(BSPNode *node, int depth) {
    if (!node)
        return;

    std::string indent(depth * 2, ' ');

    if (!node->is_leaf()) {
        if (node->first_child)
            dump_tree(node->first_child.get(), depth + 1);
        if (node->second_child)
            dump_tree(node->second_child.get(), depth + 1);
    }
}

SplitType BSPTree::determine_split_type(BSPNode *node) {
    int actual_depth = 0;
    BSPNode *current = node;
    for (; current->parent; current = current->parent, actual_depth++)
        ;

    return (actual_depth % 2 == 0) ? SplitType::HORIZONTAL
                                   : SplitType::VERTICAL;
}

BSPNode *BSPTree::find_resize_sibling(BSPNode *node) {
    if (!node || !node->parent)
        return nullptr;

    BSPNode *parent = node->parent;
    BSPNode *sibling = (parent->first_child.get() == node)
                           ? parent->second_child.get()
                           : parent->first_child.get();

    return sibling;
}

float BSPTree::calculate_ratio_from_resize(BSPNode *parent, BSPNode *child,
                                           const wlr_box &new_geo) {
    if (!parent || parent->is_leaf())
        return -1.0f;

    bool is_first = parent->first_child.get() == child;
    if (parent->split == SplitType::HORIZONTAL) {
        int new_width = new_geo.width;
        int total_width = parent->geometry.width;

        if (total_width <= 0)
            return -1.0f;

        return is_first
                   ? static_cast<float>(new_width) / total_width
                   : static_cast<float>(total_width - new_width) / total_width;
    } else if (parent->split == SplitType::VERTICAL) {
        int new_height = new_geo.height;
        int total_height = parent->geometry.height;

        if (total_height <= 0)
            return -1.0f;

        return is_first ? static_cast<float>(new_height) / total_height
                        : static_cast<float>(total_height - new_height) /
                              total_height;
    }

    return -1.0f;
}

BSPNode *BSPTree::find_resize_parent(Toplevel *toplevel, uint32_t edges) {
    BSPNode *node = find_node(toplevel);
    if (!node || !node->parent)
        return nullptr;

    BSPNode *parent = node->parent;
    bool is_first = parent->first_child.get() == node;

    if (parent->split == SplitType::HORIZONTAL) {
        if (is_first && (edges & WLR_EDGE_RIGHT))
            return parent;
        if (!is_first && (edges & WLR_EDGE_LEFT))
            return parent;
    } else if (parent->split == SplitType::VERTICAL) {
        if (is_first && (edges & WLR_EDGE_BOTTOM))
            return parent;
        if (!is_first && (edges & WLR_EDGE_TOP))
            return parent;
    }

    if (parent->parent)
        return find_resize_parent_recursive(parent, edges);

    return nullptr;
}

BSPNode *BSPTree::find_resize_parent_recursive(BSPNode *node, uint32_t edges) {
    if (!node || !node->parent)
        return nullptr;

    BSPNode *parent = node->parent;
    bool is_first = parent->first_child.get() == node;

    if (parent->split == SplitType::HORIZONTAL) {
        if (is_first && (edges & WLR_EDGE_RIGHT))
            return parent;
        if (!is_first && (edges & WLR_EDGE_LEFT))
            return parent;
    } else if (parent->split == SplitType::VERTICAL) {
        if (is_first && (edges & WLR_EDGE_BOTTOM))
            return parent;
        if (!is_first && (edges & WLR_EDGE_TOP))
            return parent;
    }

    return find_resize_parent_recursive(parent, edges);
}

void BSPTree::handle_interactive_resize(Toplevel *toplevel, uint32_t edges,
                                        int cursor_x, int cursor_y,
                                        const wlr_box &bounds) {
    BSPNode *node = find_node(toplevel);
    if (!node)
        return;

    calculate_layout(root.get(), bounds);

    std::vector<std::pair<BSPNode *, BSPNode *>> parents_to_adjust;

    uint32_t current_edges = edges;
    BSPNode *current = node;
    while (current && current->parent) {
        BSPNode *parent = current->parent;
        bool is_first = parent->first_child.get() == current;

        bool should_adjust = false;
        uint32_t next_edges = 0;

        if (parent->split == SplitType::HORIZONTAL) {
            if (is_first && (current_edges & WLR_EDGE_RIGHT)) {
                should_adjust = true;
                next_edges |= WLR_EDGE_RIGHT;
            }
            if (!is_first && (current_edges & WLR_EDGE_LEFT)) {
                should_adjust = true;
                next_edges |= WLR_EDGE_LEFT;
            }
            if (current_edges & WLR_EDGE_TOP)
                next_edges |= WLR_EDGE_TOP;
            if (current_edges & WLR_EDGE_BOTTOM)
                next_edges |= WLR_EDGE_BOTTOM;
        } else if (parent->split == SplitType::VERTICAL) {
            if (is_first && (current_edges & WLR_EDGE_BOTTOM)) {
                should_adjust = true;
                next_edges |= WLR_EDGE_BOTTOM;
            }
            if (!is_first && (current_edges & WLR_EDGE_TOP)) {
                should_adjust = true;
                next_edges |= WLR_EDGE_TOP;
            }
            if (current_edges & WLR_EDGE_LEFT)
                next_edges |= WLR_EDGE_LEFT;
            if (current_edges & WLR_EDGE_RIGHT)
                next_edges |= WLR_EDGE_RIGHT;
        }

        if (should_adjust)
            parents_to_adjust.push_back({parent, current});

        current = parent;
        current_edges = next_edges;
    }

    for (auto [parent, child] : parents_to_adjust) {
        float new_ratio = calculate_ratio_from_cursor(parent, child, edges,
                                                      cursor_x, cursor_y);
        if (new_ratio >= 0.0f)
            adjust_ratio(parent, new_ratio);
    }

    dump_tree(root.get(), 0);
}

float BSPTree::calculate_ratio_from_cursor(BSPNode *parent, BSPNode *child,
                                           uint32_t edges, int cursor_x,
                                           int cursor_y) {
    if (!parent || parent->is_leaf())
        return -1.0f;

    bool is_first = parent->first_child.get() == child;

    if (parent->split == SplitType::HORIZONTAL) {
        int relative_x = cursor_x - parent->geometry.x;
        int total_width = parent->geometry.width;

        if (total_width <= 0)
            return -1.0f;

        float new_ratio = static_cast<float>(relative_x) / total_width;

        if (!is_first && (edges & WLR_EDGE_LEFT))
            return new_ratio;
        if (is_first && (edges & WLR_EDGE_RIGHT))
            return new_ratio;
    } else if (parent->split == SplitType::VERTICAL) {
        int relative_y = cursor_y - parent->geometry.y;
        int total_height = parent->geometry.height;

        if (total_height <= 0)
            return -1.0f;

        float new_ratio = static_cast<float>(relative_y) / total_height;

        if (!is_first && (edges & WLR_EDGE_TOP))
            return new_ratio;
        if (is_first && (edges & WLR_EDGE_BOTTOM))
            return new_ratio;
    }

    return -1.0f;
}
