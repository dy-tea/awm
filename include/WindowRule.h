#pragma once
#include "Toplevel.h"
#include "xdg-shell-protocol.h"
#include <regex>
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
    std::regex title_re;
    std::regex class_re;
    uint8_t title_class_present;

    size_t rule_count{0};

    int workspace{0};
    std::string output{};
    xdg_toplevel_state *toplevel_state{nullptr};
    wlr_box *geometry{};

    WindowRule(std::string title_match, std::string class_match,
               uint8_t title_class_present);
    ~WindowRule();

    void add_rule(Rules rule_name, int data);
    void add_rule(Rules rule_name, const std::string &data);
    void add_rule(Rules rule_name, xdg_toplevel_state *data);
    bool matches(Toplevel *toplevel);
    void apply(Toplevel *toplevel);
};
