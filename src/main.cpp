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

    struct Config *config = new Config("config.toml");
    if (startup_cmd)
        config->startup_commands.push_back(startup_cmd);

    struct Server *server = new Server(config);
    delete server;
}
