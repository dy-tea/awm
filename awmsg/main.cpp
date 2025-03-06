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
    vfprintf(stdout, ("ERROR: " + msg + "\n").c_str(), args);
    va_end(args);
}

void print_usage() {
    printf("%s", "Usage: awmsg [group] [commands]\n"
                 "Groups:\n"
                 "\t[h]elp\n"
                 "\t[e]xit\n"
                 "\t[o]utput\n"
                 "\t\t- [l]ist\n"
                 "\t\t- [m]odes\n"
                 "\t[w]orkspace\n"
                 "\t\t- [l]ist\n"
                 "\t[t]oplevel\n"
                 "\t\t- [l]ist\n"
                 "\t[k]eyboard\n"
                 "\t\t- [l]ist\n"
                 "\t[d]evice\n"
                 "\t\t- [l]ist\n");
}

int main(int argc, char **argv) {
    // print usage
    if (argc == 1) {
        print_usage();
        return 0;
    }

    std::string group = argv[1];

    // group help
    if (group[0] == 'h') {
        print_usage();
        return 0;
    }

    std::string message;

    // group exit
    if (group[0] == 'e')
        message = "exit";

    // group output
    if (group[0] == 'o') {
        if (argc == 2) {
            print_usage();
            return 1;
        }

        if (argv[2][0] == 'l')
            message = "o l";
        else if (argv[2][0] == 'm')
            message = "o m";
    }

    // group workspace
    if (group[0] == 'w') {
        if (argc == 2) {
            print_usage();
            return 1;
        }

        if (argv[2][0] == 'l')
            message = "w l";
    }

    // group toplevel
    if (group[0] == 't') {
        if (argc == 2) {
            print_usage();
            return 1;
        }

        if (argv[2][0] == 'l')
            message = "t l";
    }

    // group keyboard
    if (group[0] == 'k') {
        if (argc == 2) {
            print_usage();
            return 1;
        }

        if (argv[2][0] == 'l')
            message = "k l";
    }

    // group device
    if (group[0] == 'd') {
        if (argc == 2) {
            print_usage();
            return 1;
        }

        if (argv[2][0] == 'l')
            message = "d l";
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

    // create socket
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        print_err("Failed to create socket");
        return 1;
    }

    // connect to ipc socket
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/tmp/awm.sock", sizeof(addr.sun_path) - 1);

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

    // read response from ipc socket
    std::string response;
    char buffer[1024];
    int len;
    while ((len = read(fd, buffer, sizeof(buffer))) > 0)
        response.append(buffer, len);

    if (len == -1) {
        print_err("Failed to read from IPC socket");
        return 4;
    }

    if (!response.empty()) {
        // parse response json
        json response_json = json::parse(response);

        // print response
        std::cout << response_json.dump(4) << std::endl;
    }

    // close connection
    close(fd);
}
