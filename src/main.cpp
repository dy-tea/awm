#include "Server.h"

int main(int argc, char *argv[]) {
    // start logger
    wlr_log_init(WLR_DEBUG, NULL);

    // startup and config
    std::string startup_cmd = "", config_path = "";
    std::string usage =
        "Usage: %s [-s startup command] [-c config file path]\n";

    // parse command line and set values if provided
    int c;
    while ((c = getopt(argc, argv, "s:c:h")) != -1) {
        switch (c) {
        case 's':
            startup_cmd = optarg;
            break;
        case 'c':
            config_path = optarg;
            break;
        default:
            printf(usage.c_str(), argv[0]);
            return 0;
        }
    }
    if (optind < argc) {
        printf(usage.c_str(), argv[0]);
        return 0;
    }

    if (config_path == "") {
        // no command line path passed, find in default paths
        std::string paths[] = {"$HOME/.config/awm/awm.toml",
                               "$HOME/.config/awm/config.toml",
                               "$HOME/.config/awm.toml",
                               "/usr/local/share/awm/awm.toml",
                               "/usr/local/share/awm/config.toml",
                               "/etc/awm/awm.toml",
                               "/etc/awm/config.toml",
                               "./awm.toml",
                               "./config.toml"};

        for (std::string path : paths)
            if (access(path.c_str(), F_OK) == 0) {
                config_path = path;
                break;
            }
    }

    // load config
    struct Config *config;

    if (config_path == "") {
        wlr_log(WLR_INFO, "No config found, loading defaults");
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
    if (startup_cmd != "")
        config->startup_commands.push_back(startup_cmd);

    // start server
    struct Server *server = new Server(config);
    delete server;
}
