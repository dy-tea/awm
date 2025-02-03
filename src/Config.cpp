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
            auto vars = env->getStringVector();

            if (vars)
                for (std::string &var : *vars) {
                    std::regex r("[ =]+");

                    std::sregex_token_iterator it(var.begin(), var.end(), r,
                                                  -1);
                    std::sregex_iterator end;

                    if (it->length() != 2)
                        continue;

                    std::string key = *it++;
                    std::string val = *it;

                    startup_env.emplace_back(std::make_pair(key, val));
                }
        }
    } else {
        wlr_log(WLR_INFO, "No startup configuration found, ingoring");
    }

    // Get awm binds
    std::unique_ptr<toml::Table> binds = config_file.table->getTable("binds");
    if (binds) {
        // TODO
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
