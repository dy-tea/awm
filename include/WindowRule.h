#pragma once
#include "Toplevel.h"
#include "xdg-shell-protocol.h"
#include <string>

enum Rules {
    RULES_INITIAL_WORKSPACE,
    RULES_INITIAL_OUTPUT,
    RULES_INITIAL_TOPLEVEL_STATE,
};

struct WindowRule {
    std::string title_match{};
    std::string class_match{};

    size_t rule_count{0};

    int workspace{-1};
    std::string output{};
    xdg_toplevel_state *toplevel_state{nullptr};

    WindowRule(std::string title_match, std::string class_match);
    ~WindowRule();

    void add_rule(Rules rule_name, int data);
    void add_rule(Rules rule_name, const std::string &data);
    void add_rule(Rules rule_name, xdg_toplevel_state *data);
    bool matches(Toplevel *toplevel);
    void apply(Toplevel *toplevel);
};
