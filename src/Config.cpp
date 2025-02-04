#include "Server.h"
#include "tomlcpp.hpp"
#include <sstream>

Config::Config() {}

Config::Config(std::string path) {
    // Read in config file
    toml::Result config_file = toml::parseFile(path);

    if (!config_file.table) {
        wlr_log(WLR_ERROR, "Could not parse config file, %s",
                config_file.errmsg.c_str());
        return;
    }

    // Get startup table
    std::unique_ptr<toml::Table> startup =
        config_file.table->getTable("startup");
    if (startup) {
        std::unique_ptr<toml::Array> exec = startup->getArray("exec");

        if (exec) {
            auto commands = exec->getStringVector();
            if (commands)
                for (std::string &command : *commands)
                    startup_commands.emplace_back(command);
        }

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

    // Get awm binds
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
                    if (Bind *parsed = parse_bind(bind.second))
                        commands.emplace_back(*parsed, exec.second);
            }

    } else {
        wlr_log(WLR_INFO, "No user-defined commands set, ignoring");
    }
}

Config::~Config() {}

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
                wlr_log(WLR_ERROR, "No such keycode or modifier '%s'",
                        token.c_str());
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
        if (Bind *parsed = parse_bind(row.second))
            *target = *parsed;
}
