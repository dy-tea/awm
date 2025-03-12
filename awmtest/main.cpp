#include <iostream>
#include <nlohmann/json.hpp>
#include <unistd.h>
using json = nlohmann::json;

std::string awm_executable = "./build/awm";
std::string awmsg_executable = "./build/awmsg";

// execute command and get output
std::string exec(const std::string &cmd) {
    std::cout << "EXECUTING: " << cmd << std::endl;
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"),
                                                  pclose);
    if (!pipe)
        throw std::runtime_error("popen() failed!");
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
        result += buffer.data();
    return result;
}
void exec0(const std::string &cmd) {
    std::cout << "EXECUTING: " << cmd << std::endl;
    if (!fork())
        execl("/bin/sh", "/bin/sh", "-c", cmd.c_str(), nullptr);
}

// run awmsg command and get resulting json
json awmsg(std::string command) {
    std::string result = exec(awmsg_executable + " " + command);
    return json::parse(result);
}

// spawn a command
void spawn(std::string command) { awmsg("s " + command); }

void test_fullscreen() {
    // spawn a terminal
    spawn("alacritty");

    json toplevels = awmsg("t l");

    // fullscreen 10 times
    for (int i = 0; i != 10; i++) {
        awmsg("b r fullscreen");
        sleep(1);
    }

    json toplevels2 = awmsg("t l");

    assert(toplevels == toplevels2);

    // close all toplevels
    awmsg("b r close");
}

int main() {
    // open awm
    exec0(awm_executable + " -c config.toml");

    sleep(3);

    test_fullscreen();
}
