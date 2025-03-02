#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

// IPC client

int main() {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1)
        return 1;

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/tmp/awm.sock", sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)))
        return 2;

    const char *msg = "Hello, IPC!";
    if (write(fd, msg, strlen(msg)) == -1)
        return 3;

    close(fd);
}
