#include "../test.h"

// toplevels should be the same size as the output when fullscreened
int main() {
    DEFAULT(1);

    // fullscreen
    AWMSG("b r fullscreen");

    // get toplevel bounds
    AWMSG_J("t l", toplevels);
    json toplevel = *toplevels.begin();

    // get output bounds
    AWMSG_J("o l", outputs);
    json output = *outputs.begin();

    // assertions
    ASSERT(toplevel["fullscreen"] == true);
    ASSERT(output["layout"]["width"] == toplevel["width"]);
    ASSERT(output["layout"]["height"] == toplevel["height"]);
    ASSERT(output["layout"]["x"] == toplevel["x"]);
    ASSERT(output["layout"]["y"] == toplevel["y"]);

    EXIT();
}
