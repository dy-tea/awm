#include <iostream>
#include <nlohmann/json.hpp>
#include <unistd.h>
using json = nlohmann::json;

std::string awm_executable = "./build/awm";
std::string awmsg_executable = "./build/awmsg";

// execute command and get output
std::string exec1(const std::string &cmd) {
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
void exec0(const std::string &cmd) {
    std::cout << "EXECUTING: " << cmd << std::endl;
    if (fork() == 0)
        execl("/bin/sh", "/bin/sh", "-c", cmd.c_str(), nullptr);
}

// run awmsg command and get resulting json
json awmsg(std::string command, bool data) {
    if (data) {
        std::string result = exec1(awmsg_executable + " " + command);
        return json::parse(result);
    }
    exec0(awmsg_executable + " " + command);
    return json();
}

// spawn a command
void spawn(std::string command) { awmsg("s \"" + command + "\"", false); }

void test_fullscreen_10() {
    // spawn a terminal
    spawn("alacritty");

    sleep(1);

    json toplevels = awmsg("t l", true);

    std::cout << toplevels.dump(4) << std::endl;

    // fullscreen 10 times
    for (int i = 0; i != 10; i++) {
        sleep(1);
        awmsg("b r fullscreen", false);
    }

    sleep(1);

    json toplevels2 = awmsg("t l", true);

    std::cout << toplevels2.dump(4) << std::endl;

    assert(toplevels == toplevels2);

    sleep(1);

    // close all toplevels
    awmsg("b r close", false);
}

void test_fullscreen_size() {
    // spawn a terminal
    spawn("alacritty");

    sleep(1);

    // fullscreen
    awmsg("b r fullscreen", false);

    sleep(1);

    // get toplevel bounds
    json toplevels = awmsg("t l", true);
    json toplevel = *toplevels.begin();
    std::cout << toplevel.dump(4) << std::endl;

    sleep(1);

    // get output bounds
    json outputs = awmsg("o l", true);
    json output = *outputs.begin();
    std::cout << output.dump(4) << std::endl;

    // assertions
    assert(toplevel["fullscreen"] == true);
    assert(output["width"] == toplevel["width"]);
    assert(output["height"] == toplevel["height"]);
    assert(output["x"] == toplevel["x"]);
    assert(output["y"] == toplevel["y"]);

    sleep(1);

    // close all toplevels
    awmsg("b r close", false);
}

int main() {
    // open awm
    exec0(awm_executable + " -c config.toml");

    sleep(3);

    test_fullscreen_10();
    sleep(1);
    test_fullscreen_size();
    sleep(1);

    // exit
    awmsg("e", false);
}
