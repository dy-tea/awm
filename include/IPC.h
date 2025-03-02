#include <atomic>
#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

struct IPC {
    struct Server *server;
    int fd;
    sockaddr_un addr{};
    std::string path{"/tmp/awm.sock"};
    std::atomic<bool> running{true};
    std::thread thread;

    IPC(Server *server);

    std::string run(std::string command);
    void stop();
};
