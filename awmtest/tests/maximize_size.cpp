#include "../test.h"

// maximized toplevels should take up the entire output if usable area has not
// changed due to layer surfaces
int main() {
    DEFAULT(1);

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
    ASSERT(output["usable"]["width"] == toplevel["width"]);
    ASSERT(output["usable"]["height"] == toplevel["height"]);
    ASSERT(output["usable"]["x"] == toplevel["x"]);
    ASSERT(output["usable"]["y"] == toplevel["y"]);

    EXIT();
}
