#include <iostream>
#include <nlohmann/json.hpp>
#include <unistd.h>
using json = nlohmann::json;

// awmtest dependencies
// - ./build/awm
// - ./build/awmsg
// - alacritty (can be changed to any wayland toplevel)
// - waybar (this is to test layer shell interaction with toplevels)
// - tmatrix (this is because the test will not run automatically if it is
// not receiving input, the animation forces updates)
//
// currently the tests require the cursor to be moving at all times or some
// animation playing for the binds being run to be accepted. not entirely sure
// what causes this.

std::string awm_executable = "./build/awm";
std::string awmsg_executable = "./build/awmsg";
std::string terminal_executable = "alacritty -e tmatrix";

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
}

int main() {
    const std::string awm_default = awm_executable + " -c config.toml";

    // test fullscreen
    {
        // open awm
        exec0(awm_default);
        sleep(1);

        // spawn a terminal
        spawn(terminal_executable);

        // tests
        sleep(1);
        test_fullscreen_10();
        sleep(1);
        test_fullscreen_size();
        sleep(1);

        // exit
        awmsg("e", false);
    }

    // test maximize
    {
        // open awm
        exec0(awm_default);

        // give time to position cursor on awm
        sleep(3);

        // spawn a terminal
        spawn(terminal_executable);

        // tests
        sleep(1);
        test_maximize_10();
        sleep(1);
        test_maximize_size();
        sleep(1);

        // exit
        awmsg("e", false);
    }
}
