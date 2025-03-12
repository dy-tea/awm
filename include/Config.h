#include "tomlcpp.hpp"
#include "util.h"
#include "wlr.h"
#include <filesystem>
#include <libinput.h>
#include <vector>

const std::string MODIFIERS[] = {
    "Shift", "Caps", "Control", "Alt", "Mod2", "Mod3", "Logo", "Mod5",
};

enum BindName {
    BIND_NONE,
    BIND_EXIT,
    BIND_WINDOW_FULLSCREEN,
    BIND_WINDOW_PREVIOUS,
    BIND_WINDOW_NEXT,
    BIND_WINDOW_MOVE,
    BIND_WINDOW_UP,
    BIND_WINDOW_DOWN,
    BIND_WINDOW_LEFT,
    BIND_WINDOW_RIGHT,
    BIND_WINDOW_CLOSE,
    BIND_WINDOW_SWAP_UP,
    BIND_WINDOW_SWAP_DOWN,
    BIND_WINDOW_SWAP_LEFT,
    BIND_WINDOW_SWAP_RIGHT,
    BIND_WORKSPACE_TILE,
    BIND_WORKSPACE_OPEN,
    BIND_WORKSPACE_WINDOW_TO,
};

const std::string BIND_NAMES[] = {
    "none",      "exit",      "fullscreen", "previous", "next",  "move",
    "up",        "down",      "left",       "right",    "close", "swap_up",
    "swap_down", "swap_left", "swap_right", "tile",     "open",  "window_to",
};

const std::string MOUSE_BUTTONS[] = {
    "MouseLeft", "MouseRight", "MouseMiddle", "MouseBack", "MouseForward",
};

struct Bind {
    BindName name;
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
        struct {
            libinput_config_accel_profile profile{
                LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE};
            double accel_speed{0.0};
            bool natural_scroll{true};
            int64_t left_handed{0};
        } mouse;

        struct {
            libinput_config_tap_state tap_to_click{LIBINPUT_CONFIG_TAP_ENABLED};
            libinput_config_drag_state tap_and_drag{
                LIBINPUT_CONFIG_DRAG_ENABLED};
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
            libinput_config_scroll_method scroll_method{
                LIBINPUT_CONFIG_SCROLL_2FG};
            libinput_config_click_method click_method{
                LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS};
            int event_mode{LIBINPUT_CONFIG_SEND_EVENTS_ENABLED};
            libinput_config_accel_profile profile{
                LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE};
            double accel_speed{0.0};
        } touchpad;
    } cursor;

    // compostior binds
    std::vector<Bind> binds;

    // outputs
    std::vector<OutputConfig *> outputs;

    Config();
    explicit Config(const std::string &path);
    ~Config() = default;

    void set_bind(const std::string &name, toml::Table *source,
                  const BindName target);
    bool load();
    void update(const struct Server *server);
};
