#include "tomlcpp.hpp"
#include "wlr.h"
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

    // move the active toplevel with the mouse
    struct Bind window_move{WLR_MODIFIER_ALT, XKB_KEY_m};

    // focus the window above the active one
    struct Bind window_up{WLR_MODIFIER_ALT, XKB_KEY_k};

    // focus the window below the active one
    struct Bind window_down{WLR_MODIFIER_ALT, XKB_KEY_j};

    // focus the window left of the active one
    struct Bind window_left{WLR_MODIFIER_ALT, XKB_KEY_h};

    // focus the window right of the active one
    struct Bind window_right{WLR_MODIFIER_ALT, XKB_KEY_l};

    // close the active toplevel
    struct Bind window_close{WLR_MODIFIER_ALT, XKB_KEY_q};

    // swap active and above toplevels
    struct Bind window_swap_up{WLR_MODIFIER_ALT | WLR_MODIFIER_SHIFT,
                               XKB_KEY_K};

    // swap active and below toplevels
    struct Bind window_swap_down{WLR_MODIFIER_ALT | WLR_MODIFIER_SHIFT,
                                 XKB_KEY_J};

    // swap active and left toplevels
    struct Bind window_swap_left{WLR_MODIFIER_ALT | WLR_MODIFIER_SHIFT,
                                 XKB_KEY_H};

    // swap active and above toplevels
    struct Bind window_swap_right{WLR_MODIFIER_ALT | WLR_MODIFIER_SHIFT,
                                  XKB_KEY_L};

    // set workspace to tile
    struct Bind workspace_tile{WLR_MODIFIER_ALT, XKB_KEY_t};

    // open workspace n
    struct Bind workspace_open{WLR_MODIFIER_ALT, XKB_KEY_NoSymbol};

    // move active toplevel to workspace n
    struct Bind workspace_window_to{WLR_MODIFIER_ALT | WLR_MODIFIER_SHIFT,
                                    XKB_KEY_NoSymbol};

    Config();
    Config(std::string path);
    ~Config();

    void set_bind(std::string name, toml::Table *source, Bind *target);
    struct Bind *parse_bind(std::string definition);
};
