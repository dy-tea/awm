#include <fcntl.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
using json = nlohmann::json;

void print_err(std::string msg, ...) {
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, ("ERROR: " + msg + "\n").c_str(), args);
    va_end(args);
}

// stolen from https://stackoverflow.com/a/4654718
bool is_number(const std::string &s) {
    std::string::const_iterator it = s.begin();
    while (it != s.end() && std::isdigit(*it))
        ++it;
    return !s.empty() && it == s.end();
}

void print_usage() {
    printf("%s", "Usage: awmsg [flags] [group] [subcommand]\n"
                 "flags:\n"
                 "\t-c --continuous\n"
                 "groups:\n"
                 "\t[h]elp\n"
                 "\t[e]xit\n"
                 "\t[s]pawn <command>\n"
                 "\t[o]utput\n"
                 "\t\t- [l]ist\n"
                 "\t\t- [m]odes\n"
                 "\t[w]orkspace\n"
                 "\t\t- [l]ist\n"
                 "\t\t- [s]et <num>\n"
                 "\t[t]oplevel\n"
                 "\t\t- [l]ist\n"
                 "\t[k]eyboard\n"
                 "\t\t- [l]ist\n"
                 "\t[d]evice\n"
                 "\t\t- [l]ist\n"
                 "\t\t- [c]urrent\n"
                 "\t[b]ind\n"
                 "\t\t- [l]ist\n"
                 "\t\t- [r]un <name>\n"
                 "\t\t- [d]isplay <name>\n");
}

int arg_index = 0;
std::string next(int argc, char **argv) {
    if (argc <= ++arg_index) {
        print_err("Expected argument after '%s'", argv[arg_index - 1]);
        print_usage();
        exit(1);
    }
    return std::string(argv[arg_index]);
}

int main(int argc, char **argv) {
    // print usage
    if (argc == 1) {
        print_usage();
        return 0;
    }

    std::string group = next(argc, argv), message = "";
    bool continuous = false;

    // get continuous flag if present
    if (group == "-c" || group == "--continuous") {
        continuous = true;
        group = next(argc, argv);
    }

    // parse the group
    switch (group[0]) {
    case 'h': // help
        print_usage();
        return 0;
    case 's': // spawn
        // commnd is required
        if (argc == arg_index + 1) {
            print_err("Expected argument after 's'");
            return 1;
        }

        // get command
        message = "s ";
        while (argc > arg_index + 1)
            message += " " + next(argc, argv);
        break;
    case 'e': // exit
        message = "exit";
        break;
    case 'o': // output
        group = next(argc, argv);

        if (group[0] == 'l') { // output list
            message = "o l";
            break;
        } else if (group[0] == 'm') { // output modes
            message = "o m";
            break;
        }
        goto unknown;
    case 'w': // workspace
        group = next(argc, argv);

        if (group[0] == 'l') { // workspace list
            message = "w l";
            break;
        } else if (group[0] == 's') { // workspace set
            group = next(argc, argv);

            if (!is_number(group)) {
                print_err("Argument '%s' is not a number", group.c_str());
                print_usage();
                return 1;
            }

            message = "w s " + group;
            break;
        }
        goto unknown;
    case 't': // toplevel
        group = next(argc, argv);

        if (group[0] == 'l') { // toplevel list
            message = "t l";
            break;
        }
        goto unknown;
    case 'k': // keyboard
        group = next(argc, argv);

        if (group[0] == 'l') { // keyboard list
            message = "k l";
            break;
        }
        goto unknown;
    case 'd': // device
        group = next(argc, argv);

        if (group[0] == 'l') { // device list
            message = "d l";
            break;
        } else if (group[0] == 'c') { // device current
            message = "d c";
            break;
        }
        goto unknown;
    case 'b': // bind
        group = next(argc, argv);

        if (group[0] == 'l') { // bind list
            message = "b l";
            break;
        } else if (group[0] == 'r') { // bind run
            group = next(argc, argv);
            message = "b r " + group;
            break;
        } else if (group[0] == 'd') { // bind display
            group = next(argc, argv);
            message = "b d " + group;
            break;
        }
        [[fallthrough]];
    default:
    unknown:
        // invalid group or command
        std::string query = argv[1];
        for (int i = 2; i < argc; i++)
            query += " " + std::string(argv[i]);

        print_err("Query '%s' did not match command", query.c_str());
        print_usage();
        return 1;
    }

    // add continuous flag if present
    if (continuous)
        message = "c " + message;

    // create socket
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        print_err("Failed to create socket");
        return 1;
    }

    // set socket address
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/tmp/awm.sock", sizeof(addr.sun_path) - 1);

    // connect to ipc socket
    if (connect(fd, reinterpret_cast<struct sockaddr *>(&addr),
                sizeof(struct sockaddr_un))) {
        print_err("Failed to connect to IPC socket (is awm ipc running?)");
        return 2;
    }

    // write to ipc socket
    if (write(fd, message.c_str(), strlen(message.c_str())) == -1) {
        print_err("Failed to write to IPC socket");
        return 3;
    }

    // set non blocking mode
    fcntl(fd, F_SETFL, O_NONBLOCK);

    do {
        // set read fds
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(fd, &read_fds);

        // set timeout
        struct timeval timeout;
        timeout.tv_sec = 1; // 1s timeout
        timeout.tv_usec = 0;

        // wait for response
        int ret = select(fd + 1, &read_fds, nullptr, nullptr, &timeout);
        if (ret == -1) {
            print_err("select() failed");
            break;
        } else if (!ret)
            continue;

        std::string response = "";
        if (FD_ISSET(fd, &read_fds)) {
            // read response from ipc socket
            char buffer[1024];
            int len = 0;
            while ((len = read(fd, buffer, sizeof(buffer))) > 0)
                response.append(buffer, len);

            if (!response.empty()) {
                try {
                    // parse response json
                    json response_json = json::parse(response);

                    // print response
                    std::cout << response_json.dump(4) << std::endl;

                    // clear response
                    response.clear();
                } catch (json::parse_error &e) {
                } // ignore parse errors
            }
        }
    } while (continuous);

    // close connection
    close(fd);
}
