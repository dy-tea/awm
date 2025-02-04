#include "Server.h"

int main(int argc, char *argv[]) {
    wlr_log_init(WLR_DEBUG, NULL);
    char *startup_cmd = NULL;

    int c;
    while ((c = getopt(argc, argv, "s:h")) != -1) {
        switch (c) {
        case 's':
            startup_cmd = optarg;
            break;
        default:
            printf("Usage: %s [-s startup command]\n", argv[0]);
            return 0;
        }
    }
    if (optind < argc) {
        printf("Usage: %s [-s startup command]\n", argv[0]);
        return 0;
    }

    std::string paths[] = {"$HOME/.config/awm/awm.toml",
                           "$HOME/.config/awm/config.toml",
                           "$HOME/.config/awm.toml",
                           "/etc/awm/awm.toml",
                           "/etc/awm/config.toml",
                           "/usr/local/share/awm/awm.toml",
                           "/usr/local/share/awm/config.toml",
                           "./awm.toml",
                           "./config.toml"};

    int selected_config = -1;

    for (int i = 0; i != 7; ++i)
        if (access(paths[i].c_str(), F_OK) == 0) {
            selected_config = i;
            break;
        }

    struct Config *config;

    if (selected_config == -1) {
        wlr_log(WLR_INFO, "No config found, loading defaults");
        config = new Config();
    } else {
        wlr_log(WLR_INFO, "Found config at '%s'",
                paths[selected_config].c_str());
        config = new Config(paths[selected_config]);
    }

    if (startup_cmd)
        config->startup_commands.push_back(startup_cmd);

    struct Server *server = new Server(config);
    delete server;
}
