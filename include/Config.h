#include "tomlcpp.hpp"
#include "wlr.h"
#include <iostream>
#include <vector>

struct Bind {
    uint32_t modifiers{0};
    xkb_keysym_t sym;

    bool operator==(const Bind other) {
        return modifiers == other.modifiers && sym == other.sym;
    }
};

struct Config {
    std::vector<std::string> startup_commands;
    std::vector<std::pair<std::string, std::string>> startup_env;
    std::vector<std::pair<Bind, std::string>> commands;

    // exit compositor
    struct Bind exit{WLR_MODIFIER_ALT, XKB_KEY_Escape};

    // fullscreen the active toplevel
    struct Bind window_fullscreen{WLR_MODIFIER_ALT, XKB_KEY_f};

    // focus the previous toplevel in the active workspace
    struct Bind window_previous{WLR_MODIFIER_ALT, XKB_KEY_o};

    // focus the next toplevel in the active workspace
    struct Bind window_next{WLR_MODIFIER_ALT, XKB_KEY_p};

    // set workspace to tile
    struct Bind workspace_tile{WLR_MODIFIER_ALT, XKB_KEY_t};

    // open workspace n
    struct Bind workspace_open{WLR_MODIFIER_ALT, XKB_KEY_NoSymbol};

    // move active toplevel to workspace n
    struct Bind workspace_window_to{WLR_MODIFIER_ALT | WLR_MODIFIER_SHIFT,
                                    XKB_KEY_NoSymbol};

    Config(std::string path);
    ~Config();

    void set_bind(std::string name, toml::Table *source, Bind *target);
    struct Bind *parse_bind(std::string definition);
};
