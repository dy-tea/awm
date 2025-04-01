#include "../test.h"

// toplevels should be the same size as the output when fullscreened
int main() {
    DEFAULT(1);

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
    ASSERT(toplevel["fullscreen"] == true);
    ASSERT(output["layout"]["width"] == toplevel["width"]);
    ASSERT(output["layout"]["height"] == toplevel["height"]);
    ASSERT(output["layout"]["x"] == toplevel["x"]);
    ASSERT(output["layout"]["y"] == toplevel["y"]);

    EXIT();
}
