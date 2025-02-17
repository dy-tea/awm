#include "Server.h"
#include "tomlcpp.hpp"
#include "wlr.h"
#include <sstream>

Config::Config() {
    path = "";
    last_write_time = std::filesystem::file_time_type::min();
    load();
}

Config::Config(std::string path) {
    // path
    this->path = path;

    // get last write time
    last_write_time = std::filesystem::last_write_time(path);

    // load config at path
    load();
}

Config::~Config() {}

// load config from path
bool Config::load() {
    // Read in config file
    toml::Result config_file = toml::parseFile(path);

    // false if no config file
    if (!config_file.table) {
        notify_send("Could not parse config file, %s",
                    config_file.errmsg.c_str());
        return false;
    }

    // Get startup table
    std::unique_ptr<toml::Table> startup =
        config_file.table->getTable("startup");
    if (startup) {
        // startup commands
        std::unique_ptr<toml::Array> exec = startup->getArray("exec");

        if (exec) {
            auto commands = exec->getStringVector();
            if (commands)
                for (std::string &command : *commands)
                    // add each one to startup commands
                    startup_commands.emplace_back(command);
        }

        // env vars
        std::unique_ptr<toml::Array> env = startup->getArray("env");

        if (env) {
            auto tables = env->getTableVector();

            if (tables)
                for (toml::Table table : *tables) {
                    std::vector<std::string> keys = table.keys();

                    for (std::string key : keys) {
                        // try both string and int values
                        auto intval = table.getInt(key);
                        auto stringval = table.getString(key);

                        // add each to startup env
                        if (intval.first)
                            startup_env.emplace_back(
                                key, std::to_string(intval.second));
                        else if (stringval.first)
                            startup_env.emplace_back(key, stringval.second);
                    }
                }
        }
    } else {
        wlr_log(WLR_INFO, "No startup configuration found, ingoring");
    }

    // get keyboard config
    std::unique_ptr<toml::Table> keyboard =
        config_file.table->getTable("keyboard");
    if (keyboard) {
        struct xkb_rule_names names;

        // get layout
        auto layout = keyboard->getString("layout");
        if (layout.first) {
            keyboard_layout = layout.second;
            wlr_log(WLR_INFO, "Keyboard layout set to: %s", names.layout);
        }

        // get model
        auto model = keyboard->getString("model");
        if (model.first) {
            keyboard_model = model.second;
            wlr_log(WLR_INFO, "Keyboard model set to: %s", names.model);
        }

        // get variant
        auto variant = keyboard->getString("variant");
        if (variant.first) {
            keyboard_variant = variant.second;
            wlr_log(WLR_INFO, "Keyboard variant set to: %s", names.variant);
        }

        // repeat rate
        auto rate = keyboard->getInt("repeat_rate");
        if (rate.first) {
            repeat_rate = rate.second;
            wlr_log(WLR_INFO, "Keyboard repeat rate set to: %d", repeat_rate);
        }

        // repeat delay
        auto delay = keyboard->getInt("repeat_delay");
        if (delay.first) {
            repeat_delay = delay.second;
            wlr_log(WLR_INFO, "Keyboard repeat delay set to: %d", repeat_delay);
        }
    } else
        // no keyboard config
        wlr_log(WLR_INFO, "no keyboard configuration found, using us layout");

    // get awm binds
    std::unique_ptr<toml::Table> binds = config_file.table->getTable("binds");
    if (binds) {
        // exit bind
        set_bind("exit", binds.get(), &exit);

        // window binds
        auto window_bind = binds->getTable("window");
        if (window_bind) {
            // window_fullscreen bind
            set_bind("fullscreen", window_bind.get(), &window_fullscreen);

            // window_previous bind
            set_bind("previous", window_bind.get(), &window_previous);

            // window_next bind
            set_bind("next", window_bind.get(), &window_next);

            // window_move bind
            set_bind("move", window_bind.get(), &window_move);

            // window_up bind
            set_bind("up", window_bind.get(), &window_up);

            // window_down bind
            set_bind("down", window_bind.get(), &window_down);

            // window_left bind
            set_bind("left", window_bind.get(), &window_left);

            // window_right bind
            set_bind("right", window_bind.get(), &window_right);

            // window_close bind
            set_bind("close", window_bind.get(), &window_close);

            auto swap_bind = window_bind->getTable("swap");
            if (swap_bind) {
                // window_swap_up bind
                set_bind("up", swap_bind.get(), &window_swap_up);

                // window_swap_down bind
                set_bind("down", swap_bind.get(), &window_swap_down);

                // window_swap_left bind
                set_bind("left", swap_bind.get(), &window_swap_left);

                // window_swap_right bind
                set_bind("right", swap_bind.get(), &window_swap_right);
            }
        }

        // workspace binds
        auto workspace_bind = binds->getTable("workspace");
        if (workspace_bind) {
            // workspace_tile bind
            set_bind("tile", workspace_bind.get(), &workspace_tile);

            // workspace_open bind
            set_bind("open", workspace_bind.get(), &workspace_open);

            // workspace_window_to bind
            set_bind("window_to", workspace_bind.get(), &workspace_window_to);
        }
    } else {
        wlr_log(WLR_INFO, "No binds set, using defaults");
    }

    // Get user-defined commands
    std::unique_ptr<toml::Array> command_tables =
        config_file.table->getArray("commands");
    if (command_tables) {
        auto tables = command_tables->getTableVector();
        if (tables)
            for (toml::Table &table : *tables.get()) {
                auto bind = table.getString("bind");
                auto exec = table.getString("exec");

                if (bind.first && exec.first)
                    if (Bind *parsed = parse_bind(bind.second)) {
                        commands.emplace_back(*parsed, exec.second);
                        delete parsed;
                    }
            }

    } else {
        wlr_log(WLR_INFO, "No user-defined commands set, ignoring");
    }

    // monitor configs
    std::unique_ptr<toml::Array> monitor_tables =
        config_file.table->getArray("monitors");
    if (monitor_tables) {
        auto tables = monitor_tables->getTableVector();
        if (tables)
            for (toml::Table &table : *tables.get()) {
                // create new output config
                OutputConfig *oc = new OutputConfig();

                // name
                connect(table.getString("name"), &oc->name);

                // enabled
                connect(table.getBool("enabled"), &oc->enabled);

                // width
                connect<int32_t>(table.getInt("width"), &oc->width);

                // height
                connect<int32_t>(table.getInt("height"), &oc->height);

                // x
                connect(table.getDouble("x"), &oc->x);

                // y
                connect(table.getDouble("y"), &oc->y);

                // refresh rate
                connect(table.getDouble("refresh"), &oc->refresh);

                // transform
                auto transform = table.getString("transform");
                std::string transform_values[] = {
                    "none", "90", "180", "270", "f", "f90", "f180", "f270"};
                if (transform.first)
                    for (int i = 0; i != WL_OUTPUT_TRANSFORM_FLIPPED_270 + 1;
                         ++i)
                        if (transform.second == transform_values[i]) {
                            oc->transform = (wl_output_transform)i;
                            break;
                        }

                // scale
                connect(table.getDouble("scale"), &oc->scale);

                // adaptive sync
                connect(table.getBool("adaptive"), &oc->adaptive_sync);

                // add to output configs if enough values are set
                if (oc->name.empty() || !oc->width || !oc->height ||
                    oc->refresh <= 0.0) {
                    notify_send("monitor config is missing one of the required "
                                "fields: name, width, height, refresh");
                    delete oc;
                } else {
                    wlr_log(WLR_INFO, "added monitor config for %s: %dx%d@%.1f",
                            oc->name.c_str(), oc->width, oc->height,
                            oc->refresh);
                    outputs.emplace_back(oc);
                }
            }
    }

    return true;
}

// get the wlr modifier enum value from the string representation
uint32_t parse_modifier(std::string modifier) {
    const std::string modifiers[] = {
        "Shift", "Caps", "Control", "Alt", "Mod2", "Mod3", "Logo", "Mod5",
    };

    for (int i = 0; i != 8; ++i)
        if (modifier == modifiers[i])
            return 1 << i;

    return 69;
}

// create a bind from a space-seperated string of modifiers and key
struct Bind *Config::parse_bind(std::string definition) {
    Bind *bind = new Bind;

    std::string token;
    std::stringstream ss(definition);

    while (std::getline(ss, token, ' ')) {
        if (token == "Number") {
            bind->sym = XKB_KEY_NoSymbol;
            continue;
        }

        xkb_keysym_t sym =
            xkb_keysym_from_name(token.c_str(), XKB_KEYSYM_NO_FLAGS);

        if (sym == XKB_KEY_NoSymbol) {
            uint32_t modifier = parse_modifier(token);

            if (modifier == 69) {
                notify_send("No such keycode or modifier '%s'", token.c_str());
                return nullptr;
            }

            bind->modifiers |= modifier;
        } else
            bind->sym = sym;
    }

    return bind;
}

// helper function to bind a variable from a given table and row name
void Config::set_bind(std::string name, toml::Table *source, Bind *target) {
    auto row = source->getString(name);
    if (row.first)
        if (Bind *parsed = parse_bind(row.second)) {
            *target = *parsed;
            delete parsed;
        }
}

// helper function to connect the second pair if the first bool is true
template <typename T> void Config::connect(std::pair<bool, T> pair, T *target) {
    if (pair.first)
        *target = pair.second;
}

// update the config
void Config::update(struct Server *server) {
    // get current write time
    std::filesystem::file_time_type current_write_time =
        std::filesystem::last_write_time(path);

    // check if the file has been modified
    if (current_write_time == last_write_time)
        return;

    // update write time
    last_write_time = current_write_time;

    // load new config
    if (!load())
        return;

    // update keyboard config
    struct wlr_keyboard *wlr_keyboard = wlr_seat_get_keyboard(server->seat);
    Keyboard *keyboard = static_cast<Keyboard *>(wlr_keyboard->data);
    keyboard->update_config();
}
