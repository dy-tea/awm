#include "Config.h"
#include "Cursor.h"
#include "Keyboard.h"
#include "Seat.h"
#include "Server.h"
#include "util.h"
#include <libinput.h>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>

// get the wlr modifier enum value from the string representation
uint32_t parse_modifier(const std::string &modifier) {
    for (int i = 0; i != 8; ++i)
        if (modifier == MODIFIERS[i])
            return 1 << i;

    // invalid modifier
    return 69420;
}

// create a bind from a space-seperated string of modifiers and key
Bind *parse_bind(const std::string &definition, BindName name) {
    Bind *bind = new Bind{name, 0, 0};

    std::string token;
    std::stringstream ss(definition);

cont:

    // get each space-seperated token
    while (std::getline(ss, token, ' ')) {
        // special handling for number keys
        if (token == "Number") {
            bind->sym = XKB_KEY_NoSymbol;
            continue;
        }

        // mouse buttons
        for (int i = 0; i != 5; ++i)
            if (token == MOUSE_BUTTONS[i]) {
                bind->sym = 0x20000000 + i + 272;
                goto cont; // can't continue here due to for loop
            }

        // parse keysym
        const xkb_keysym_t sym =
            xkb_keysym_from_name(token.c_str(), XKB_KEYSYM_NO_FLAGS);

        if (sym == XKB_KEY_NoSymbol) {
            // not a keysym, parse modifier
            const uint32_t modifier = parse_modifier(token);

            if (modifier == 69420) {
                // not a keysym or modifier, tell user
                notify_send("Config", "No such keycode or modifier '%s'",
                            token.c_str());
                delete bind;
                return nullptr;
            }

            // add modifier
            bind->modifiers |= modifier;
        } else
            // add keysym
            bind->sym = sym;
    }

    return bind;
}

// add bind to binds if valid
void Config::set_bind(const std::string &name, const toml::Table *source,
                      const BindName target) {
    if (!source)
        return;

    if (auto value = source->getString(name)) {
        if (const Bind *bind = parse_bind(*value, target)) {
            binds.emplace_back(*bind);
            delete bind;
        }
    }
}

// Helper to map string options to enum values
template <typename T>
std::optional<T> map_option(const std::string &name,
                            const std::unordered_map<std::string, T> &options,
                            const std::optional<std::string> &value) {
    if (!value)
        return std::nullopt;

    auto it = options.find(*value);
    if (it != options.end())
        return it->second;

    // Build error message with available options
    std::string opts_str;
    for (const auto &[key, _] : options) {
        if (!opts_str.empty())
            opts_str += "', '";
        opts_str += key;
    }

    notify_send("Config", "No such option in %s ['%s']: %s", name.c_str(),
                opts_str.c_str(), value->c_str());
    return std::nullopt;
}

Config::Config()
    : path(""), last_write_time(std::filesystem::file_time_type::min()) {
    notify_send("Config", "%s",
                "no config loaded, press Alt+Escape to exit awm");
}

Config::Config(const std::string &path) : path(path) {
    // get last write time
    last_write_time = std::filesystem::last_write_time(path);

    // load config at path
    load();
}

// load config from path
bool Config::load() {
    // read in config file
    toml::ParseResult config_file = toml::parseFile(path);

    // false if no config file
    if (!config_file) {
        notify_send("Config", "Could not parse config file, %s",
                    config_file.error.c_str());
        return false;
    }

    const toml::Table *root = config_file.table.get();

    // get startup table
    if (const toml::Table *startup = root->getTable("startup")) {
        // startup commands
        if (const toml::Array *exec = startup->getArray("exec")) {
            auto commands = exec->getStringVector();
            startup_commands.clear();
            startup_commands = std::move(commands);
        }

        // env vars
        if (const toml::Array *env = startup->getArray("env")) {
            auto tables = env->getTableVector();
            startup_env.clear();

            for (const toml::Table *table : tables) {
                auto keys = table->keys();

                for (const std::string &key : keys) {
                    // try both string and int values
                    if (auto intval = table->getInt(key)) {
                        startup_env.emplace_back(key, std::to_string(*intval));
                    } else if (auto stringval = table->getString(key)) {
                        startup_env.emplace_back(key, *stringval);
                    }
                }
            }
        }
    } else {
        startup_env.clear();
        startup_commands.clear();
    }

    // exit
    if (const toml::Table *exit_table = root->getTable("exit")) {
        // list of commands to run on exit
        if (const toml::Array *exec = exit_table->getArray("exec")) {
            auto commands = exec->getStringVector();
            exit_commands.clear();
            exit_commands = std::move(commands);
        }
    } else {
        exit_commands.clear();
    }

    // ipc
    if (const toml::Table *ipc_table = root->getTable("ipc")) {
        ipc.path = ipc_table->get<std::string>("socket", "");
        ipc.enabled = ipc_table->get<bool>("enabled", true);
        ipc.spawn = ipc_table->get<bool>("spawn", true);
        ipc.bind_run = ipc_table->get<bool>("bind_run", true);
    } else {
        ipc.path = "";
        ipc.enabled = true;
        ipc.spawn = true;
        ipc.bind_run = true;
    }

    // get keyboard config
    if (const toml::Table *keyboard = root->getTable("keyboard")) {
        keyboard_layout = keyboard->get<std::string>("layout", "us");
        keyboard_model = keyboard->get<std::string>("model", "");
        keyboard_variant = keyboard->get<std::string>("variant", "");
        keyboard_options = keyboard->get<std::string>("options", "");
        repeat_rate = keyboard->get<int64_t>("repeat_rate", 25);
        repeat_delay = keyboard->get<int64_t>("repeat_delay", 600);
    } else {
        keyboard_layout = "us";
        keyboard_model = "";
        keyboard_variant = "";
        keyboard_options = "";
        repeat_rate = 25;
        repeat_delay = 600;

        wlr_log(WLR_INFO, "%s",
                "no keyboard configuration found, using us layout with default "
                "settings");
    }

    // get pointer config
    if (const toml::Table *pointer = root->getTable("pointer")) {
        // xcursor
        if (const toml::Table *xcursor_table = pointer->getTable("xcursor")) {
            cursor.xcursor.theme = xcursor_table->get<std::string>("theme", "");
            cursor.xcursor.size = xcursor_table->get<int64_t>("size", 24);
        }

        // mouse
        if (const toml::Table *mouse = pointer->getTable("mouse")) {
            cursor.mouse.natural_scroll =
                mouse->get<bool>("natural_scroll", false);
            cursor.mouse.left_handed = mouse->get<bool>("left_handed", false);
            cursor.mouse.accel_speed = mouse->get<double>("accel_speed", 0.0);

            // profile
            static const std::unordered_map<std::string,
                                            libinput_config_accel_profile>
                profile_map = {
                    {"none", LIBINPUT_CONFIG_ACCEL_PROFILE_NONE},
                    {"flat", LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT},
                    {"adaptive", LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE},
                };
            cursor.mouse.profile =
                map_option("pointer.mouse.profile", profile_map,
                           mouse->getString("profile"))
                    .value_or(LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE);
        }

        // touchpad
        if (const toml::Table *touchpad = pointer->getTable("touchpad")) {
            // tap to click
            if (auto tap_to_click = touchpad->getBool("tap_to_click")) {
                cursor.touchpad.tap_to_click =
                    static_cast<libinput_config_tap_state>(*tap_to_click);
            }

            // tap and drag
            if (auto tap_and_drag = touchpad->getBool("tap_and_drag")) {
                cursor.touchpad.tap_and_drag =
                    static_cast<libinput_config_drag_state>(*tap_and_drag);
            }

            // drag lock
            static const std::unordered_map<std::string,
                                            libinput_config_drag_lock_state>
                drag_lock_map = {
                    {"none", LIBINPUT_CONFIG_DRAG_LOCK_DISABLED},
                    {"timeout", LIBINPUT_CONFIG_DRAG_LOCK_ENABLED_TIMEOUT},
                    {"sticky", LIBINPUT_CONFIG_DRAG_LOCK_ENABLED_STICKY},
                };
            cursor.touchpad.drag_lock =
                map_option("pointer.touchpad.drag_lock", drag_lock_map,
                           touchpad->getString("drag_lock"))
                    .value_or(LIBINPUT_CONFIG_DRAG_LOCK_ENABLED_STICKY);

            // tap button map
            static const std::unordered_map<std::string,
                                            libinput_config_tap_button_map>
                tap_button_map_map = {
                    {"lrm", LIBINPUT_CONFIG_TAP_MAP_LRM},
                    {"lmr", LIBINPUT_CONFIG_TAP_MAP_LMR},
                };
            cursor.touchpad.tap_button_map =
                map_option("pointer.touchpad.tap_button_map",
                           tap_button_map_map,
                           touchpad->getString("tap_button_map"))
                    .value_or(LIBINPUT_CONFIG_TAP_MAP_LRM);

            cursor.touchpad.natural_scroll =
                touchpad->get<bool>("natural_scroll", true);

            // disable while typing
            if (auto disable_while_typing =
                    touchpad->getBool("disable_while_typing")) {
                cursor.touchpad.disable_while_typing =
                    static_cast<libinput_config_dwt_state>(
                        *disable_while_typing);
            }

            cursor.touchpad.left_handed =
                touchpad->get<bool>("left_handed", false);

            // middle emulation
            if (auto middle_emulation = touchpad->getBool("middle_emulation")) {
                cursor.touchpad.middle_emulation =
                    static_cast<libinput_config_middle_emulation_state>(
                        *middle_emulation);
            }

            // scroll method
            static const std::unordered_map<std::string,
                                            libinput_config_scroll_method>
                scroll_method_map = {
                    {"none", LIBINPUT_CONFIG_SCROLL_NO_SCROLL},
                    {"2fg", LIBINPUT_CONFIG_SCROLL_2FG},
                    {"edge", LIBINPUT_CONFIG_SCROLL_EDGE},
                    {"button", LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN},
                };
            cursor.touchpad.scroll_method =
                map_option("pointer.touchpad.scroll_method", scroll_method_map,
                           touchpad->getString("scroll_method"))
                    .value_or(LIBINPUT_CONFIG_SCROLL_2FG);

            // click method
            static const std::unordered_map<std::string,
                                            libinput_config_click_method>
                click_method_map = {
                    {"none", LIBINPUT_CONFIG_CLICK_METHOD_NONE},
                    {"buttonareas", LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS},
                    {"clickfinger", LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER},
                };
            cursor.touchpad.click_method =
                map_option("pointer.touchpad.click_method", click_method_map,
                           touchpad->getString("click_method"))
                    .value_or(LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS);

            // event mode
            static const std::unordered_map<std::string, int> event_mode_map = {
                {"enabled", LIBINPUT_CONFIG_SEND_EVENTS_ENABLED},
                {"disabled", LIBINPUT_CONFIG_SEND_EVENTS_DISABLED},
                {"mousedisabled",
                 LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE},
            };
            cursor.touchpad.event_mode =
                map_option("pointer.touchpad.event_mode", event_mode_map,
                           touchpad->getString("event_mode"))
                    .value_or(LIBINPUT_CONFIG_SEND_EVENTS_ENABLED);

            // profile
            static const std::unordered_map<std::string,
                                            libinput_config_accel_profile>
                profile_map = {
                    {"none", LIBINPUT_CONFIG_ACCEL_PROFILE_NONE},
                    {"flat", LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT},
                    {"adaptive", LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE},
                };
            cursor.touchpad.profile =
                map_option("pointer.touchpad.profile", profile_map,
                           touchpad->getString("profile"))
                    .value_or(LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE);

            cursor.touchpad.accel_speed =
                touchpad->get<double>("accel_speed", 0.0);
        }
    } else {
        cursor.xcursor.theme = "";
        cursor.xcursor.size = 24;

        cursor.mouse.profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
        cursor.mouse.accel_speed = 0.0;
        cursor.mouse.natural_scroll = false;
        cursor.mouse.left_handed = false;

        cursor.touchpad.tap_to_click = LIBINPUT_CONFIG_TAP_ENABLED;
        cursor.touchpad.tap_and_drag = LIBINPUT_CONFIG_DRAG_ENABLED;
        cursor.touchpad.drag_lock = LIBINPUT_CONFIG_DRAG_LOCK_ENABLED_STICKY;
        cursor.touchpad.tap_button_map = LIBINPUT_CONFIG_TAP_MAP_LRM;
        cursor.touchpad.natural_scroll = true;
        cursor.touchpad.disable_while_typing = LIBINPUT_CONFIG_DWT_ENABLED;
        cursor.touchpad.left_handed = false;
        cursor.touchpad.middle_emulation =
            LIBINPUT_CONFIG_MIDDLE_EMULATION_DISABLED;
        cursor.touchpad.scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;
        cursor.touchpad.click_method =
            LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
        cursor.touchpad.event_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
        cursor.touchpad.profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
        cursor.touchpad.accel_speed = 0.0;

        wlr_log(WLR_INFO, "%s",
                "no cursor configuration found, using defaults");
    }

    // general
    if (const toml::Table *general_table = root->getTable("general")) {
        general.focus_on_hover =
            general_table->get<bool>("focus_on_hover", false);

        // focus on activation
        static const std::unordered_map<std::string, FocusOnWindowActivation>
            fowa_map = {
                {"none", FOWA_NONE},
                {"active", FOWA_ACTIVE},
                {"any", FOWA_ANY},
            };
        general.fowa =
            map_option("general.focus_on_activation", fowa_map,
                       general_table->getString("focus_on_activation"))
                .value_or(FOWA_ACTIVE);

        general.system_bell =
            general_table->get<std::string>("system_bell", "");
        general.minimize_to_workspace =
            general_table->get<int64_t>("minimize_to_workspace", 0);
        general.decorations = general_table->get<bool>("decorations", true);
    } else {
        general.focus_on_hover = false;
        general.fowa = FOWA_ACTIVE;
        general.system_bell = "";
        general.minimize_to_workspace = 0;
        general.decorations = true;

        wlr_log(WLR_INFO, "%s",
                "no general configuration found, using defaults");
    }

    // tiling
    if (const toml::Table *tiling_table = root->getTable("tiling")) {
        // method
        static const std::unordered_map<std::string, TileMethod> method_map = {
            {"none", TILE_NONE},     {"grid", TILE_GRID},
            {"master", TILE_MASTER}, {"dwindle", TILE_DWINDLE},
            {"bsp", TILE_BSP},
        };
        tiling.method = map_option("tiling.method", method_map,
                                   tiling_table->getString("method"))
                            .value_or(TILE_GRID);

        tiling.auto_tile = tiling_table->get<bool>("auto_tile", false);
        tiling.float_on_min_size = tiling_table->get<bool>("float_on_min_size", false);
        tiling.float_on_max_size = tiling_table->get<bool>("float_on_max_size", false);
        tiling.float_on_both = tiling_table->get<bool>("float_on_both", false);
    } else {
        tiling.method = TILE_GRID;
        tiling.auto_tile = false;
        tiling.float_on_min_size = false;
        tiling.float_on_max_size = false;
        tiling.float_on_both = false;
    }

    // get awm binds
    if (const toml::Table *binds_table = root->getTable("binds")) {
        // clear binds
        binds.clear();

        // exit bind
        set_bind("exit", binds_table, BIND_EXIT);

        // ensure exit bind is available
        if (binds.empty()) {
            binds.push_back(Bind{BIND_EXIT, WLR_MODIFIER_ALT, XKB_KEY_Escape});
            notify_send("Config", "%s",
                        "No exit bind set, press Alt+Escape to exit awm");
        }

        // window binds
        if (const toml::Table *window_bind = binds_table->getTable("window")) {
            set_bind("maximize", window_bind, BIND_WINDOW_MAXIMIZE);
            set_bind("fullscreen", window_bind, BIND_WINDOW_FULLSCREEN);
            set_bind("previous", window_bind, BIND_WINDOW_PREVIOUS);
            set_bind("next", window_bind, BIND_WINDOW_NEXT);
            set_bind("move", window_bind, BIND_WINDOW_MOVE);
            set_bind("resize", window_bind, BIND_WINDOW_RESIZE);
            set_bind("pin", window_bind, BIND_WINDOW_PIN);
            set_bind("toggle_floating", window_bind, BIND_WINDOW_TOGGLE_FLOATING);
            set_bind("up", window_bind, BIND_WINDOW_UP);
            set_bind("down", window_bind, BIND_WINDOW_DOWN);
            set_bind("left", window_bind, BIND_WINDOW_LEFT);
            set_bind("right", window_bind, BIND_WINDOW_RIGHT);
            set_bind("close", window_bind, BIND_WINDOW_CLOSE);

            // swap binds
            if (const toml::Table *swap_bind = window_bind->getTable("swap")) {
                set_bind("up", swap_bind, BIND_WINDOW_SWAP_UP);
                set_bind("down", swap_bind, BIND_WINDOW_SWAP_DOWN);
                set_bind("left", swap_bind, BIND_WINDOW_SWAP_LEFT);
                set_bind("right", swap_bind, BIND_WINDOW_SWAP_RIGHT);
            }

            // half binds
            if (const toml::Table *half_bind = window_bind->getTable("half")) {
                set_bind("up", half_bind, BIND_WINDOW_HALF_UP);
                set_bind("down", half_bind, BIND_WINDOW_HALF_DOWN);
                set_bind("left", half_bind, BIND_WINDOW_HALF_LEFT);
                set_bind("right", half_bind, BIND_WINDOW_HALF_RIGHT);
            }
        }

        // workspace binds
        if (const toml::Table *workspace_bind =
                binds_table->getTable("workspace")) {
            set_bind("tile", workspace_bind, BIND_WORKSPACE_TILE);
            set_bind("tile_sans", workspace_bind, BIND_WORKSPACE_TILE_SANS);
            set_bind("auto_tile", workspace_bind, BIND_WORKSPACE_AUTO_TILE);
            set_bind("open", workspace_bind, BIND_WORKSPACE_OPEN);
            set_bind("window_to", workspace_bind, BIND_WORKSPACE_WINDOW_TO);
        }
    }

    // Get user-defined commands
    if (const toml::Array *command_tables = root->getArray("commands")) {
        commands.clear();

        auto tables = command_tables->getTableVector();
        for (const toml::Table *table : tables) {
            auto bind = table->getString("bind");
            auto exec = table->getString("exec");

            if (bind && exec) {
                if (Bind *parsed = parse_bind(*bind, BIND_NONE)) {
                    commands.emplace_back(*parsed, *exec);
                    delete parsed;
                }
            }
        }
    } else {
        wlr_log(WLR_INFO, "%s", "No user-defined commands set, ignoring");
    }

    // monitor configs
    if (const toml::Array *monitor_tables = root->getArray("monitors")) {
        outputs.clear();

        auto tables = monitor_tables->getTableVector();
        for (const toml::Table *table : tables) {
            // create new output config
            auto *oc = new OutputConfig();

            oc->name = table->get<std::string>("name", "");
            oc->enabled = table->get<bool>("enabled", true);
            oc->width = static_cast<int32_t>(table->get<int64_t>("width", 0));
            oc->height = static_cast<int32_t>(table->get<int64_t>("height", 0));

            // x and y can be int or double
            if (auto x_int = table->getInt("x"))
                oc->x = static_cast<double>(*x_int);
            else
                oc->x = table->get<double>("x", 0.0);

            if (auto y_int = table->getInt("y"))
                oc->y = static_cast<double>(*y_int);
            else
                oc->y = table->get<double>("y", 0.0);

            // refresh can be int or double
            if (auto refresh_int = table->getInt("refresh"))
                oc->refresh = static_cast<double>(*refresh_int);
            else
                oc->refresh = table->get<double>("refresh", 0.0);

            // transform
            static const std::unordered_map<std::string, wl_output_transform>
                transform_map = {
                    {"none", WL_OUTPUT_TRANSFORM_NORMAL},
                    {"90", WL_OUTPUT_TRANSFORM_90},
                    {"180", WL_OUTPUT_TRANSFORM_180},
                    {"270", WL_OUTPUT_TRANSFORM_270},
                    {"f", WL_OUTPUT_TRANSFORM_FLIPPED},
                    {"f90", WL_OUTPUT_TRANSFORM_FLIPPED_90},
                    {"f180", WL_OUTPUT_TRANSFORM_FLIPPED_180},
                    {"f270", WL_OUTPUT_TRANSFORM_FLIPPED_270},
                };
            oc->transform = map_option("monitors.transform", transform_map,
                                       table->getString("transform"))
                                .value_or(WL_OUTPUT_TRANSFORM_NORMAL);

            // scale can be int or double
            if (auto scale_int = table->getInt("scale"))
                oc->scale = static_cast<float>(*scale_int);
            else
                oc->scale = static_cast<float>(table->get<double>("scale", 1.0));

            if (auto adaptive = table->getBool("adaptive"))
                oc->adaptive_sync = *adaptive;

            oc->allow_tearing = table->get<bool>("tearing", false);

            // render bit depth
            static const std::unordered_map<std::string, RenderBitDepth>
                bit_depth_map = {
                    {"none", RENDER_BIT_DEPTH_DEFAULT},
                    {"6", RENDER_BIT_DEPTH_6},
                    {"8", RENDER_BIT_DEPTH_8},
                    {"10", RENDER_BIT_DEPTH_10},
                };
            oc->render_bit_depth =
                map_option("monitors.render_bit_depth", bit_depth_map,
                           table->getString("render_bit_depth"))
                    .value_or(RENDER_BIT_DEPTH_DEFAULT);

            oc->hdr = table->get<bool>("hdr", true);
            oc->max_render_time = static_cast<uint32_t>(
                table->get<int64_t>("max_render_time", 0));

            // add to output configs if enough values are set
            if (oc->name.empty() || !oc->width || !oc->height ||
                oc->refresh <= 0.0) {
                notify_send("Config", "%s",
                            "monitor config is missing one of the required "
                            "fields: name, width, height, refresh");
                delete oc;
            } else {
                wlr_log(WLR_INFO, "added monitor config for %s: %dx%d@%.1f",
                        oc->name.c_str(), oc->width, oc->height, oc->refresh);
                outputs.emplace_back(oc);
            }
        }
    }

    // window rule configs
    if (const toml::Array *window_rule_tables = root->getArray("windowrules")) {
        window_rules.clear();

        auto tables = window_rule_tables->getTableVector();
        for (const toml::Table *table : tables) {
            auto title_result = table->getString("title");
            auto class_result = table->getString("class");
            auto tag_result = table->getString("tag");

            if (!(title_result || class_result || tag_result)) {
                notify_send("Config", "%s",
                            "windowrules must have title, class or tag");
                continue;
            }

            // create new WindowRule
            WindowRule *w = new WindowRule(
                title_result.value_or(""), class_result.value_or(""),
                tag_result.value_or(""),
                (title_result.has_value() ? 1 : 0) |
                    (class_result.has_value() ? 2 : 0) |
                    (tag_result.has_value() ? 4 : 0));

            // workspace
            if (auto workspace = table->getInt("workspace")) {
                w->workspace = static_cast<int>(*workspace);
            }

            // output
            if (auto output = table->getString("output")) {
                w->output = *output;
            }

            // state
            if (auto state_source = table->getString("state")) {
                w->toplevel_state = new xdg_toplevel_state;
                static const std::unordered_map<std::string, xdg_toplevel_state>
                    state_map = {
                        {"maximized", XDG_TOPLEVEL_STATE_MAXIMIZED},
                        {"fullscreen", XDG_TOPLEVEL_STATE_FULLSCREEN},
                    };
                if (auto state = map_option("windowrules.state", state_map,
                                            state_source)) {
                    *w->toplevel_state = *state;
                }
            }

            // toplevel pinned
            if (auto pinned = table->getBool("pinned")) {
                w->pinned = *pinned;
            }

            // toplevel floating
            if (auto floating = table->getBool("floating")) {
                w->floating = *floating;
            }

            // tiling mode
            if (auto tiling_mode = table->getString("tiling_mode")) {
                static const std::unordered_map<std::string, TilingMode> tiling_mode_map = {
                    {"auto", TILING_MODE_AUTO},
                    {"floating", TILING_MODE_FLOATING},
                    {"tiling", TILING_MODE_TILING},
                };
                if (auto mode = map_option("windowrules.tiling_mode", tiling_mode_map, tiling_mode))
                    w->tiling_mode = *mode;
            }

            // geometry
            if (const toml::Table *geometry_table =
                    table->getTable("geometry")) {
                w->geometry->x =
                    static_cast<int>(geometry_table->get<int64_t>("x", 0));
                w->geometry->y =
                    static_cast<int>(geometry_table->get<int64_t>("y", 0));
                w->geometry->width =
                    static_cast<int>(geometry_table->get<int64_t>("width", 0));
                w->geometry->height =
                    static_cast<int>(geometry_table->get<int64_t>("height", 0));
            } else {
                delete w->geometry;
                w->geometry = nullptr;
            }

            window_rules.emplace_back(w);
        }
    }

    return true;
}

Config::~Config() {
    for (auto *output : outputs)
        delete output;

    for (auto *rule : window_rules)
        delete rule;
}

void Config::update(const struct Server *server) {
    // check if config file has been modified
    if (!std::filesystem::exists(path))
        return;

    const std::filesystem::file_time_type current_write_time =
        std::filesystem::last_write_time(path);

    if (current_write_time == last_write_time)
        return;

    last_write_time = current_write_time;

    wlr_log(WLR_INFO, "%s", "config file modified, reloading");

    // reload config
    load();

    // update keyboard and cursor config
    const wlr_keyboard *wlr_keyboard =
        wlr_seat_get_keyboard(server->seat->wlr_seat);
    const auto *keyboard = reinterpret_cast<Keyboard *>(wlr_keyboard->data);

    keyboard->update_config();
    server->cursor->reconfigure_all();

    notify_send("Config", "%s", "config reload complete");
}
