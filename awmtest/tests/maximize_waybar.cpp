#include "../test.h"

// waybar will change usable area due it being a layer surface, the toplevel
// should maximize accordinly
int main() {
    DEFAULT(1);

    // spawn a waybar
    spawn("waybar");
    sleep(1);

    // maximize
    AWMSG("b r maximize");

    // get toplevel bounds
    AWMSG_J("t l", toplevels);
    json toplevel = *toplevels.begin();

    // get output bounds
    AWMSG_J("o l", outputs);
    json output = *outputs.begin();

    // assertions
    ASSERT(toplevel["maximized"] == true);
    ASSERT(toplevel["x"] == output["usable"]["x"]);
    ASSERT(toplevel["y"] == output["usable"]["y"]);
    ASSERT(toplevel["width"] == output["usable"]["width"]);
    ASSERT(toplevel["height"] == output["usable"]["height"]);

    EXIT();
}
