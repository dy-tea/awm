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
    struct Bind window_fullscreen{};

    // focus the previous toplevel in the active workspace
    struct Bind window_previous{};

    // focus the next toplevel in the active workspace
    struct Bind window_next{};

    // move the active toplevel with the mouse
    struct Bind window_move{};

    // focus the window above the active one
    struct Bind window_up{};

    // focus the window below the active one
    struct Bind window_down{};

    // focus the window left of the active one
    struct Bind window_left{};

    // focus the window right of the active one
    struct Bind window_right{};

    // close the active toplevel
    struct Bind window_close{};

    // swap active and above toplevels
    struct Bind window_swap_up{};

    // swap active and below toplevels
    struct Bind window_swap_down{};

    // swap active and left toplevels
    struct Bind window_swap_left{};

    // swap active and above toplevels
    struct Bind window_swap_right{};

    // set workspace to tile
    struct Bind workspace_tile{};

    // open workspace n
    struct Bind workspace_open{};

    // move active toplevel to workspace n
    struct Bind workspace_window_to{};

    std::vector<OutputConfig *> outputs;

    Config();
    explicit Config(const std::string &path);
    ~Config() = default;

    bool load();

    void update(const struct Server *server);
};
