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

    std::string group = next(argc, argv);
    bool continuous = false;

    // get continuous flag if present
    if (group == "-c" || group == "--continuous") {
        continuous = true;
        group = next(argc, argv);
    }

    // group help
    if (group[0] == 'h') {
        print_usage();
        return 0;
    }

    std::string message = "";

    // group spawn
    if (group[0] == 's') {
        group = next(argc, argv);

        message = "s " + group;
    }

    // group exit
    if (group[0] == 'e')
        message = "exit";

    // group output
    if (group[0] == 'o') {
        group = next(argc, argv);

        if (group[0] == 'l')
            message = "o l";
        else if (group[0] == 'm')
            message = "o m";
    }

    // group workspace
    if (group[0] == 'w') {
        group = next(argc, argv);

        if (group[0] == 'l')
            message = "w l";
        else if (group[0] == 's') {
            group = next(argc, argv);

            if (!is_number(group)) {
                print_err("Argument '%s' is not a number", group.c_str());
                print_usage();
                return 1;
            }

            message = "w s " + group;
        }
    }

    // group toplevel
    if (group[0] == 't') {
        group = next(argc, argv);

        if (group[0] == 'l')
            message = "t l";
    }

    // group keyboard
    if (group[0] == 'k') {
        group = next(argc, argv);

        if (group[0] == 'l')
            message = "k l";
    }

    // group device
    if (group[0] == 'd') {
        group = next(argc, argv);

        if (group[0] == 'l')
            message = "d l";
        else if (group[0] == 'c')
            message = "d c";
    }

    // group bind
    if (group[0] == 'b') {
        group = next(argc, argv);

        if (group[0] == 'l')
            message = "b l";
        else if (group[0] == 'r') {
            group = next(argc, argv);
            message = "b r " + group;
        } else if (group[0] == 'd') {
            group = next(argc, argv);
            message = "b d " + group;
        }
    }

    // invalid group or command
    if (message == "") {
        std::string query = argv[1];
        for (int i = 2; i < argc; i++)
            query += " " + std::string(argv[i]);

        print_err("Query '%s' did not match command", query.c_str());
        print_usage();
        return 0;
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
