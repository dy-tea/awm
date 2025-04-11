#include "Server.h"
#include "version.h"
#include <wordexp.h>

Server *Server::instance = nullptr;

int main(const int argc, char *argv[]) {
    // start logger
    wlr_log_init(WLR_DEBUG, nullptr);

    // startup and config
    std::string startup_cmd, config_path;
    const std::string usage =
        "Usage: %s [-v] [-s startup command] [-c config file path]\n";

    // parse command line and set values if provided
    int c;
    while ((c = getopt(argc, argv, "s:c:h:v")) != -1) {
        switch (c) {
        case 's':
            startup_cmd = optarg;
            break;
        case 'c':
            config_path = optarg;
            break;
        case 'v':
            printf("%s\n", AWM_VERSION);
            return 0;
        default:
            printf(usage.c_str(), argv[0]);
            return 0;
        }
    }
    if (optind < argc) {
        printf(usage.c_str(), argv[0]);
        return 0;
    }

    if (config_path.empty()) {
        wordexp_t p = {0, nullptr, 0};

        // no command line path passed, find in default paths
        std::string paths[] = {"$XDG_CONFIG_HOME/awm/awm.toml",
                               "$XDG_CONFIG_HOME/awm/config.toml",
                               "$XDG_CONFIG_HOME/awm.toml",
                               "$HOME/.config/awm/awm.toml",
                               "$HOME/.config/awm/config.toml",
                               "$HOME/.config/awm.toml",
                               "/usr/local/share/awm/awm.toml",
                               "/usr/local/share/awm/config.toml",
                               "/etc/awm/awm.toml",
                               "/etc/awm/config.toml",
                               "./awm.toml",
                               "./config.toml"};

        for (const std::string &path : paths) {
            if (wordexp(path.c_str(), &p, p.we_wordc != 0 ? WRDE_REUSE : 0) !=
                0)
                continue; // ignore expansion failure
            if (access(p.we_wordv[0], F_OK) == 0) {
                config_path = p.we_wordv[0];
                break;
            }
        }

        if (p.we_wordc != 0)
            wordfree(&p);
    }

    // load config
    Config *config;

    if (config_path.empty()) {
        wlr_log(WLR_INFO, "%s", "No config found, loading defaults");
        config = new Config();
    } else if (access(config_path.c_str(), F_OK) == 0) {
        wlr_log(WLR_INFO, "Loading config at '%s'", config_path.c_str());
        config = new Config(config_path);
    } else {
        wlr_log(WLR_ERROR, "Config file '%s' does not exist",
                config_path.c_str());
        return 1;
    }

    // add startup command
    if (!startup_cmd.empty())
        config->startup_commands.push_back(startup_cmd);

    // start server
    Server *server = Server::get(config);
    delete server;
    delete config;
}
