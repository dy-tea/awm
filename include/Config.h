#include "tomlcpp.hpp"
#include "util.h"
#include "wlr.h"
#include <filesystem>
#include <libinput.h>
#include <vector>

struct Bind {
    uint32_t modifiers{0};
    xkb_keysym_t sym;

    bool operator==(const Bind other) const {
        return modifiers == other.modifiers && sym == other.sym;
    }
};

struct OutputConfig {
    std::string name;
    bool enabled{true};
    int32_t width{0}, height{0};
    double x{0.0}, y{0.0};
    double refresh{0.0};
    enum wl_output_transform transform { WL_OUTPUT_TRANSFORM_NORMAL };
    double scale{1.0};
    bool adaptive_sync{false};

    OutputConfig() = default;

    explicit OutputConfig(const wlr_output_configuration_head_v1 *config_head) {
        enabled = config_head->state.enabled;

        if (config_head->state.mode) {
            wlr_output_mode *mode = config_head->state.mode;
            width = mode->width;
            height = mode->height;
            refresh = mode->refresh / 1000.0;
        } else {
            width = config_head->state.custom_mode.width;
            height = config_head->state.custom_mode.height;
            refresh = config_head->state.custom_mode.refresh / 1000.0;
        }
        x = config_head->state.x;
        y = config_head->state.y;
        transform = config_head->state.transform;
        scale = config_head->state.scale;
        adaptive_sync = config_head->state.adaptive_sync_enabled;
    }
};

struct Config {
    std::string path;
    std::filesystem::file_time_type last_write_time;

    std::string renderer{"auto"};
    std::vector<std::string> startup_commands;
    std::vector<std::string> exit_commands;
    std::vector<std::pair<std::string, std::string>> startup_env;
    std::vector<std::pair<Bind, std::string>> commands;
    bool ipc{true};

    // keyboard
    std::string keyboard_layout{"us"};
    std::string keyboard_model;
    std::string keyboard_variant;
    std::string keyboard_options;
    int64_t repeat_rate{25}, repeat_delay{600};

    // cursor
    struct {
        libinput_config_tap_state tap_to_click{LIBINPUT_CONFIG_TAP_ENABLED};
        libinput_config_drag_state tap_and_drag{LIBINPUT_CONFIG_DRAG_ENABLED};
        libinput_config_drag_lock_state drag_lock{
            LIBINPUT_CONFIG_DRAG_LOCK_DISABLED};
        libinput_config_tap_button_map tap_button_map{
            LIBINPUT_CONFIG_TAP_MAP_LRM};
        bool natural_scroll{true};
        libinput_config_dwt_state disable_while_typing{
            LIBINPUT_CONFIG_DWT_ENABLED};
        int64_t left_handed{0};
        libinput_config_middle_emulation_state middle_emulation{
            LIBINPUT_CONFIG_MIDDLE_EMULATION_DISABLED};
        libinput_config_scroll_method scroll_method{LIBINPUT_CONFIG_SCROLL_2FG};
        libinput_config_click_method click_method{
            LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS};
        int event_mode{LIBINPUT_CONFIG_SEND_EVENTS_ENABLED};
        libinput_config_accel_profile profile{
            LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE};
        double accel_speed{0.0};
    } cursor;

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

    std::vector<OutputConfig *> outputs;

    Config();
    explicit Config(const std::string &path);
    ~Config() = default;

    bool load();

    void update(const struct Server *server);
};
