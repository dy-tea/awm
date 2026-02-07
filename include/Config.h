#pragma once

#include "WindowRule.h"
#include "tomlcpp.hpp"
#include "wlr.h"
#include <filesystem>
#include <vector>

const std::string MODIFIERS[] = {
    "Shift", "Caps", "Control", "Alt", "Mod2", "Mod3", "Logo", "Mod5",
};

enum BindName {
    BIND_NONE,
    BIND_EXIT,
    BIND_WINDOW_MAXIMIZE,
    BIND_WINDOW_FULLSCREEN,
    BIND_WINDOW_PREVIOUS,
    BIND_WINDOW_NEXT,
    BIND_WINDOW_MOVE,
    BIND_WINDOW_RESIZE,
    BIND_WINDOW_PIN,
    BIND_WINDOW_UP,
    BIND_WINDOW_DOWN,
    BIND_WINDOW_LEFT,
    BIND_WINDOW_RIGHT,
    BIND_WINDOW_CLOSE,
    BIND_WINDOW_SWAP_UP,
    BIND_WINDOW_SWAP_DOWN,
    BIND_WINDOW_SWAP_LEFT,
    BIND_WINDOW_SWAP_RIGHT,
    BIND_WINDOW_HALF_UP,
    BIND_WINDOW_HALF_DOWN,
    BIND_WINDOW_HALF_LEFT,
    BIND_WINDOW_HALF_RIGHT,
    BIND_WORKSPACE_TILE,
    BIND_WORKSPACE_TILE_SANS,
    BIND_WORKSPACE_AUTO_TILE,
    BIND_WORKSPACE_OPEN,
    BIND_WORKSPACE_WINDOW_TO, // change in IPC.cpp if extended
};

const std::string BIND_NAMES[] = {
    "none",      "exit",       "maximize",   "fullscreen", "previous",
    "next",      "move",       "resize",     "pin",        "up",
    "down",      "left",       "right",      "close",      "swap_up",
    "swap_down", "swap_left",  "swap_right", "half_up",    "half_down",
    "half_left", "half_right", "tile",       "tile_sans",  "auto_tile",
    "open",      "window_to",
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

enum RenderBitDepth {
    RENDER_BIT_DEPTH_DEFAULT,
    RENDER_BIT_DEPTH_6,
    RENDER_BIT_DEPTH_8,
    RENDER_BIT_DEPTH_10,
};

struct OutputConfig {
    std::string name;
    bool enabled{true};
    int32_t width{0}, height{0};
    double x{0.0}, y{0.0};
    double refresh{0.0};
    enum wl_output_transform transform { WL_OUTPUT_TRANSFORM_NORMAL };
    float scale{1.0};
    bool adaptive_sync{false};
    bool allow_tearing{false};
    bool hdr{false};
    RenderBitDepth render_bit_depth{RENDER_BIT_DEPTH_DEFAULT};
    uint32_t max_render_time{0};

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

enum TileMethod { TILE_NONE, TILE_GRID, TILE_MASTER, TILE_DWINDLE, TILE_BSP };
enum FocusOnWindowActivation { FOWA_NONE, FOWA_ACTIVE, FOWA_ANY };

struct Config {
    std::string path;
    std::filesystem::file_time_type last_write_time;

    std::vector<std::pair<std::string, std::string>> startup_env;
    std::vector<std::string> startup_commands;
    std::vector<std::string> exit_commands;
    std::vector<std::pair<Bind, std::string>> commands;

    struct {
        std::string path{""};
        bool enabled{true};
        bool spawn{true};
        bool bind_run{true};
    } ipc;

    // keyboard
    std::string keyboard_layout{"us"};
    std::string keyboard_model;
    std::string keyboard_variant;
    std::string keyboard_options;
    int64_t repeat_rate{25}, repeat_delay{600};

    // cursor
    struct {
        struct {
            std::string theme{};
            int64_t size{24};
        } xcursor;

        struct {
            libinput_config_accel_profile profile{
                LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE};
            double accel_speed{0.0};
            bool natural_scroll{false};
            bool left_handed{false};
        } mouse;

        struct {
            libinput_config_tap_state tap_to_click{LIBINPUT_CONFIG_TAP_ENABLED};
            libinput_config_drag_state tap_and_drag{
                LIBINPUT_CONFIG_DRAG_ENABLED};
            libinput_config_drag_lock_state drag_lock{
                LIBINPUT_CONFIG_DRAG_LOCK_ENABLED_STICKY};
            libinput_config_tap_button_map tap_button_map{
                LIBINPUT_CONFIG_TAP_MAP_LRM};
            bool natural_scroll{true};
            libinput_config_dwt_state disable_while_typing{
                LIBINPUT_CONFIG_DWT_ENABLED};
            bool left_handed{false};
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

    struct {
        bool focus_on_hover{false};
        FocusOnWindowActivation fowa{FOWA_ACTIVE};
        std::string system_bell{""};
        int64_t minimize_to_workspace{0};
        bool decorations{true};
    } general;

    struct {
        TileMethod method{TILE_GRID};
        bool auto_tile{false};
    } tiling;

    // compostior binds
    std::vector<Bind> binds;

    // outputs
    std::vector<OutputConfig *> outputs;

    // window rules
    std::vector<WindowRule *> window_rules;

    Config();
    explicit Config(const std::string &path);
    ~Config();

    void set_bind(const std::string &name, toml::Table *source,
                  const BindName target);
    bool load();
    void update(const struct Server *server);
};
