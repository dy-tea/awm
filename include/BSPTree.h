#pragma once

#include "wlr.h"
#include <memory>
#include <vector>

struct Toplevel;
struct Workspace;

enum class SplitType { NONE, HORIZONTAL, VERTICAL };

struct BSPNode {
    BSPNode *parent{nullptr};
    std::unique_ptr<BSPNode> first_child{nullptr};
    std::unique_ptr<BSPNode> second_child{nullptr};

    SplitType split{SplitType::NONE};
    float ratio{0.5f};

    Toplevel *toplevel{nullptr};
    wlr_box geometry{};

    BSPNode() = default;
    explicit BSPNode(Toplevel *tl) : toplevel(tl) {}

    bool is_leaf() const { return split == SplitType::NONE; }
    bool is_container() const { return !is_leaf(); }

    BSPNode *find_toplevel(Toplevel *tl);
    bool remove_toplevel(Toplevel *tl);
    void get_toplevels(std::vector<Toplevel *> &toplevels) const;
    int count_leaves() const;
    BSPNode *find_insertion_point();
};

struct BSPTree {
    std::unique_ptr<BSPNode> root{nullptr};
    Workspace *workspace{nullptr};

    explicit BSPTree(Workspace *ws) : workspace(ws) {}

    void insert(Toplevel *toplevel);
    void insert_at(Toplevel *toplevel, Toplevel *target);
    void remove(Toplevel *toplevel);
    void apply_layout(const wlr_box &bounds, bool use_transaction = true);
    BSPNode *find_node(Toplevel *toplevel);
    bool get_toplevel_geometry(Toplevel *toplevel, const wlr_box &bounds,
                               wlr_box &out_geometry);
    void adjust_ratio(BSPNode *node, float new_ratio);
    void handle_resize(Toplevel *toplevel, const wlr_box &new_geo);
    void rebuild(std::vector<Toplevel *> toplevels);
    void rebuild_grid(std::vector<Toplevel *> toplevels);
    void rebuild_dwindle(std::vector<Toplevel *> toplevels);
    void insert_at_dwindle(Toplevel *toplevel, Toplevel *target);
    void clear();

    void handle_interactive_resize(Toplevel *toplevel, uint32_t edges,
                                   int cursor_x, int cursor_y,
                                   const wlr_box &bounds);

  private:
    void calculate_layout(BSPNode *node, const wlr_box &bounds);
    void apply_geometries(BSPNode *node, bool immediate = false);
    void dump_tree(BSPNode *node, int depth);
    SplitType determine_split_type(BSPNode *node);
    BSPNode *find_resize_sibling(BSPNode *node);
    float calculate_ratio_from_resize(BSPNode *parent, BSPNode *child,
                                      const wlr_box &new_geo);
    BSPNode *find_resize_parent(Toplevel *toplevel, uint32_t edges);
    BSPNode *find_resize_parent_recursive(BSPNode *node, uint32_t edges);
    float calculate_ratio_from_cursor(BSPNode *parent, BSPNode *child,
                                      uint32_t edges, int cursor_x,
                                      int cursor_y);
};
