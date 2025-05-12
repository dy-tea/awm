#include <iostream>
#include <nlohmann/json.hpp>
#include <unistd.h>
using json = nlohmann::json;

// globals
const std::string awm_executable = "./awm";
const std::string awmsg_executable = "./awmsg -s /tmp/awm.sock";
const std::string terminal_executable = "alacritty -e tmatrix";
const std::string awm_default = awm_executable + " -c ../config.toml";

// execute command and get its output
inline std::string exec1(const std::string &cmd) {
    std::cout << "EXECUTING: " << cmd << std::endl;
    char buffer[128];
    std::string result = "";
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe)
        throw std::runtime_error("popen() failed!");
    try {
        while (fgets(buffer, sizeof buffer, pipe))
            result += buffer;
    } catch (...) {
        pclose(pipe);
        throw;
    }
    pclose(pipe);
    return result;
}

// execute command
inline void exec0(const std::string &cmd) {
    std::cout << "EXECUTING: " << cmd << std::endl;
    if (fork() == 0)
        execl("/bin/sh", "/bin/sh", "-c", cmd.c_str(), nullptr);
}

// run awmsg command and get resulting json
inline json awmsg(std::string command) {
    std::string result = exec1(awmsg_executable + " " + command);
    if (result.empty())
        return json(false);
    if (result == "null")
        return json();
    return json::parse(result);
}

// spawn a command
inline void spawn(std::string command) { awmsg("s \"" + command + "\""); }

inline void DEFAULT() {
    exec0(awm_default);
    sleep(1);
}

inline void DEFAULT(uint32_t toplevels) {
    DEFAULT();
    for (uint32_t i = 0; i != toplevels; ++i) {
        spawn(terminal_executable);
        sleep(1);
    }
}

inline void EXIT() {
    awmsg("e");
    sleep(1);
}

#define AWMSG(x)                                                               \
    {                                                                          \
        if (awmsg(x) == json(false)) {                                         \
            std::cerr << "Message did not get reply" << std::endl;             \
            return 1;                                                          \
        }                                                                      \
        sleep(1);                                                              \
    }

#define AWMSG_J(x, j)                                                          \
    json j = awmsg(x);                                                         \
    if (j == json(false)) {                                                    \
        std::cerr << "Message did not get reply" << std::endl;                 \
        return 1;                                                              \
    }                                                                          \
    std::cout << j.dump(4) << std::endl;                                       \
    sleep(1);

#define ASSERT(x)                                                              \
    if (!(x)) {                                                                \
        std::cerr << "Assertion failed: " << #x << std::endl;                  \
        EXIT();                                                                \
        exit(1);                                                               \
    }
