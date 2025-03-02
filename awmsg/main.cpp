#include <iostream>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

void print_err(std::string msg, ...) {
    va_list args;
    va_start(args, msg);
    vfprintf(stdout, ("ERROR: " + msg + "\n").c_str(), args);
    va_end(args);
}

void print_usage() {
    const std::string tab = "    ";
    std::cout << "Usage: awmsg [group] [commands]" << std::endl
              << "Groups:" << std::endl
              << tab << "[h]elp" << std::endl
              << tab << "[e]xit" << std::endl
              << tab << "[o]utput" << std::endl
              << tab << tab << "- [l]ist" << std::endl
              << tab << "[w]orkspace" << std::endl
              << tab << tab << "- [l]ist" << std::endl
              << tab << tab << "- [t]oplevels" << std::endl
              << tab << "[t]oplevel" << std::endl
              << tab << tab << "- [l]ist" << std::endl;
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

    std::string message = "";

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
            message = "output list";
    }

    // group workspace
    if (group[0] == 'w') {
        if (argc == 2) {
            print_usage();
            return 1;
        }

        if (argv[2][0] == 'l')
            message = "workspace list";
        else if (argv[2][0] == 't')
            message = "workspace toplevels";
    }

    // group toplevel
    if (group[0] == 't') {
        if (argc == 2) {
            print_usage();
            return 1;
        }

        if (argv[2][0] == 'l')
            message = "toplevel list";
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

    if (connect(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un))) {
        print_err("Failed to connect to IPC socket (is awm ipc running?)");
        return 2;
    }

    // write to ipc socket
    if (write(fd, message.c_str(), strlen(message.c_str())) == -1) {
        print_err("Failed to write to IPC socket");
        return 3;
    }

    close(fd);
}
