#pragma once

#include "wlr.h"
#include <regex>
#include <string>

enum WindowRuleFlags {
    WINDOW_RULE_TITLE = 1 << 0,
    WINDOW_RULE_CLASS = 1 << 1,
    WINDOW_RULE_TAG = 1 << 2,
};

enum Rules {
    RULES_WORKSPACE,
    RULES_OUTPUT,
    RULES_TOPLEVEL_STATE,
    RULES_TOPLEVEL_PIN,
    RULES_FLOATING,
    RULES_TOPLEVEL_X,
    RULES_TOPLEVEL_Y,
    RULES_TOPLEVEL_W,
    RULES_TOPLEVEL_H,
};

enum TilingMode {
    TILING_MODE_AUTO,
    TILING_MODE_FLOATING,
    TILING_MODE_TILING,
};

struct Toplevel;

struct WindowRule {
    std::string title, class_, tag;        // uncompiled
    std::regex title_re, class_re, tag_re; // compiled
    uint8_t matches_present;               // WindowRuleFlags

    int workspace{0};
    std::string output{};
    xdg_toplevel_state *toplevel_state{nullptr};
    bool pinned{false};
    bool floating{false};
    TilingMode tiling_mode{TILING_MODE_AUTO};
    wlr_box *geometry{nullptr};

    WindowRule(std::string title_match, std::string class_match,
               std::string tag_match, uint8_t matches_present);
    ~WindowRule();

    bool matches(Toplevel *toplevel);
    void apply(Toplevel *toplevel);
};
