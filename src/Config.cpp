#include "Server.h"
#include "tomlcpp.hpp"
#include <regex>

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
    } else {
        wlr_log(WLR_INFO, "No binds set, using defaults");
    }

    // Get user-defined commands
    std::unique_ptr<toml::Table> commands =
        config_file.table->getTable("commands");
    if (commands) {
        // TODO
    } else {
        wlr_log(WLR_INFO, "No user-defined commands set, ignoring");
    }
}

Config::~Config() {}
