#include "../test.h"

// maximized toplevels should take up the entire output if usable area has not
// changed due to layer surfaces
int main() {
    DEFAULT(1);

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

    // ASSERTions
    ASSERT(toplevel["maximized"] == true);
    ASSERT(output["usable"]["width"] == toplevel["width"]);
    ASSERT(output["usable"]["height"] == toplevel["height"]);
    ASSERT(output["usable"]["x"] == toplevel["x"]);
    ASSERT(output["usable"]["y"] == toplevel["y"]);

    EXIT();
}
