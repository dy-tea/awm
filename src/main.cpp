#include "Server.h"

int main(int argc, char *argv[]) {
    wlr_log_init(WLR_DEBUG, NULL);
    char *startup_cmd = NULL;
    char *config_path = NULL;

    std::string usage = "Usage: %s [-s startup command] [-c config file path]\n";

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
            config_path = path.data();
            break;
        }

    struct Config *config;

    if (config_path) {
        if (access(config_path, F_OK) == 0) {
            wlr_log(WLR_INFO, "Loading config at '%s'", config_path);
            config = new Config(config_path);
        } else {
            wlr_log(WLR_ERROR, "Config file '%s' ", config_path);
            return 1;
        }
    } else {
        wlr_log(WLR_INFO, "No config found, loading defaults");
        config = new Config();
    }

    if (startup_cmd)
        config->startup_commands.push_back(startup_cmd);

    struct Server *server = new Server(config);
    delete server;
}
