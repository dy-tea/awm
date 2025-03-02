#include "Server.h"

IPC::IPC(Server *server) : server(server) {
    // create file descriptor
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        wlr_log(WLR_ERROR, "failed to create IPC socket");
        return;
    }

    wlr_log(WLR_INFO, "starting IPC on socket %d", fd);

    // set socket address
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

    // bind socket
    if (bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
        wlr_log(WLR_ERROR, "failed to bind socket with fd `%d` on path `%s`",
                fd, path.c_str());
        return;
    }

    // listen for connections
    if (listen(fd, 5) == -1) {
        wlr_log(WLR_ERROR,
                "failed to listen on socket with fd `%d` on path `%s`", fd,
                path.c_str());
        return;
    }

    // start thread
    thread = std::thread([&]() {
        while (running) {
            // accept connections
            int client_fd = accept(fd, nullptr, nullptr);
            if (client_fd == -1) {
                wlr_log(WLR_ERROR,
                        "failed to accept connection on socket with fd `%d` on "
                        "path `%s`",
                        fd, path.c_str());
                continue;
            }

            // read from client
            char buffer[256];
            ssize_t n = read(client_fd, buffer, sizeof(buffer));
            if (n == -1)
                wlr_log(WLR_ERROR,
                        "failed to read from client with fd `%d` on path `%s`",
                        client_fd, path.c_str());

            wlr_log(WLR_INFO, "Received: %s", buffer);

            // close connection
            close(client_fd);
        }
    });
}

void IPC::stop() {
    // stop thread
    running = false;
    pthread_cancel(thread.native_handle());
    if (thread.joinable())
        thread.join();

    // close
    close(fd);

    // unlink
    if (unlink(path.c_str()) == 1)
        wlr_log(WLR_ERROR, "failed to unlink IPC socket at path `%s`",
                path.c_str());

    delete this;
}
