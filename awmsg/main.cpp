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

int main(int argc, char **argv) {
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
    const char *msg = "Hello, IPC!";
    if (write(fd, msg, strlen(msg)) == -1) {
        print_err("Failed to write to IPC socket");
        return 3;
    }

    close(fd);
}
