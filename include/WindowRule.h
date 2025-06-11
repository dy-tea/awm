#pragma once
#include "Toplevel.h"
#include "xdg-shell-protocol.h"
#include <string>

enum Rules {
    RULES_WORKSPACE,
    RULES_OUTPUT,
    RULES_TOPLEVEL_STATE,
    RULES_TOPLEVEL_X,
    RULES_TOPLEVEL_Y,
    RULES_TOPLEVEL_W,
    RULES_TOPLEVEL_H,
};

struct WindowRule {
    std::string title_match{};
    std::string class_match{};

    size_t rule_count{0};

    int workspace{-1};
    std::string output{};
    xdg_toplevel_state *toplevel_state{nullptr};
    wlr_box *geometry{};

    WindowRule(std::string title_match, std::string class_match);
    ~WindowRule();

    void add_rule(Rules rule_name, int data);
    void add_rule(Rules rule_name, const std::string &data);
    void add_rule(Rules rule_name, xdg_toplevel_state *data);
    bool matches(Toplevel *toplevel);
    void apply(Toplevel *toplevel);
};
