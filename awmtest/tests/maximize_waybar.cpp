#include "../test.h"

// waybar will change usable area due it being a layer surface, the toplevel
// should maximize accordinly
int main() {
    DEFAULT(1);

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
    ASSERT(toplevel["maximized"] == true);
    ASSERT(toplevel["x"] == output["usable"]["x"]);
    ASSERT(toplevel["y"] == output["usable"]["y"]);
    ASSERT(toplevel["width"] == output["usable"]["width"]);
    ASSERT(toplevel["height"] == output["usable"]["height"]);

    EXIT();
}
