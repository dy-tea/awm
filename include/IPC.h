#include <atomic>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>
#include <vector>
using json = nlohmann::json;

enum IPCMessage {
    IPC_NONE,
    IPC_EXIT,
    IPC_OUTPUT_LIST,
    IPC_OUTPUT_MODES,
    IPC_WORKSPACE_LIST,
    IPC_WORKSPACE_SET,
    IPC_TOPLEVEL_LIST,
    IPC_KEYBOARD_LIST,
    IPC_DEVICE_LIST,
    IPC_DEVICE_CURRENT,
};

struct IPC {
    struct Server *server;
    int fd;
    sockaddr_un addr{};
    std::string path{"/tmp/awm.sock"};
    std::atomic<bool> running{true};
    std::thread thread;

    IPC(Server *server);

    json handle_command(const IPCMessage message, const std::string &data);

    std::map<int, std::vector<std::pair<IPCMessage, std::string>>>
        subscriptions;
    std::mutex subscriptions_mutex;

    std::string parse_command(const std::string &command, const int client_fd,
                              const bool continuous);
    void notify_clients(const IPCMessage message);
    void stop();
};
