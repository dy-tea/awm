#include <iostream>
#include <nlohmann/json.hpp>
#include <unistd.h>
using json = nlohmann::json;

// globals
const std::string awm_executable = "./build/awm";
const std::string awmsg_executable = "./build/awmsg";
const std::string terminal_executable = "alacritty -e tmatrix";
const std::string awm_default = awm_executable + " -c config.toml";

// execute command and get its output
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

// fullscreen 10 times, the toplevel geometry should be the same
void test_fullscreen_10() {
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
}

// toplevels should be the same size as the output when fullscreened
void test_fullscreen_size() {
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
    assert(output["layout"]["width"] == toplevel["width"]);
    assert(output["layout"]["height"] == toplevel["height"]);
    assert(output["layout"]["x"] == toplevel["x"]);
    assert(output["layout"]["y"] == toplevel["y"]);
}

// maximize 10 times, the toplevel geometry should be the same
void test_maximize_10() {
    // get toplevel bounds
    json toplevel = awmsg("t l", true);

    // send maximize 10 times
    for (int i = 0; i != 10; ++i) {
        sleep(1);
        awmsg("b r maximize", false);
    }

    sleep(1);

    // get toplevel bounds
    json toplevel1 = awmsg("t l", true);

    assert(toplevel == toplevel1);
}

// maximized toplevels should take up the entire output if usable area has not
// changed due to layer surfaces
void test_maximize_size() {
    // maximize
    awmsg("b r maximize", false);

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
    assert(toplevel["maximized"] == true);
    assert(output["usable"]["width"] == toplevel["width"]);
    assert(output["usable"]["height"] == toplevel["height"]);
    assert(output["usable"]["x"] == toplevel["x"]);
    assert(output["usable"]["y"] == toplevel["y"]);

    sleep(1);

    // unmaximize
    awmsg("b r maximize", false);
}

// waybar will change usable area due it being a layer surface, the toplevel
// should maximize accordinly
void test_maximize_waybar() {
    // spawn a waybar
    spawn("waybar");
    sleep(1);

    // maximize
    awmsg("b r maximize", false);
    sleep(1);

    // get toplevel bounds
    json toplevels = awmsg("t l", true);
    json toplevel = *toplevels.begin();

    sleep(1);

    // get output bounds
    json outputs = awmsg("o l", true);
    json output = *outputs.begin();
    std::cout << output.dump(4) << std::endl;

    // assertions
    assert(toplevel["maximized"] == true);
    assert(toplevel["x"] == output["usable"]["x"]);
    assert(toplevel["y"] == output["usable"]["y"]);
    assert(toplevel["width"] == output["usable"]["width"]);
    assert(toplevel["height"] == output["usable"]["height"]);
}

void test_workpaces_10() {
    // spawn a toplevel on each workspace
    for (int i = 1; i <= 10; ++i) {
        spawn(terminal_executable);
        sleep(1);

        awmsg("w s " + std::to_string(i), false);
        sleep(1);
    }

    // get toplevels
    json toplevels = awmsg("t l", true);
    sleep(1);

    // get workspace info
    json workspaces = awmsg("w l", true);
    sleep(1);

    // assertions
    assert(toplevels.size() == 10);
    assert(workspaces["active"] == 10);
    assert(workspaces["max"] == 10);
    assert(workspaces["toplevels"] == 10);
}

int main() {
    // test fullscreen
    {
        // open awm
        exec0(awm_default);
        sleep(1);

        // spawn a terminal
        spawn(terminal_executable);
        sleep(1);

        // tests
        test_fullscreen_10();
        sleep(1);
        test_fullscreen_size();
        sleep(1);

        // exit
        awmsg("e", false);
        sleep(1);
    }

    // test maximize
    {
        // open awm
        exec0(awm_default);
        sleep(1);

        // spawn a terminal
        spawn(terminal_executable);
        sleep(1);

        // tests
        test_maximize_10();
        sleep(1);
        test_maximize_size();
        sleep(1);
        test_maximize_waybar();
        sleep(1);

        // exit
        awmsg("e", false);
        sleep(1);
    }

    // test workspaces
    {
        // open awm
        exec0(awm_default);
        sleep(1);

        // tests
        test_workpaces_10();

        // exit
        awmsg("e", false);
        sleep(1);
    }
}
