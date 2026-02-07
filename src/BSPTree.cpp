#include "BSPTree.h"
#include "Toplevel.h"
#include "Workspace.h"
#include <algorithm>

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

void BSPTree::apply_layout(const wlr_box &bounds) {
    if (!root)
        return;

    calculate_layout(root.get(), bounds);
    apply_geometries(root.get());
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
        if (tl && !tl->fullscreen())
            insert(tl);
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

void BSPTree::apply_geometries(BSPNode *node) {
    if (!node)
        return;

    if (node->is_leaf() && node->toplevel) {
        if (!node->toplevel->maximized() && !node->toplevel->fullscreen())
            node->toplevel->set_position_size(node->geometry);
        return;
    }

    if (node->first_child)
        apply_geometries(node->first_child.get());
    if (node->second_child)
        apply_geometries(node->second_child.get());
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
