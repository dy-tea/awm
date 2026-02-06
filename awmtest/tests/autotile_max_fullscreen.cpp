#include "../test.h"

// test that auto-tiling works after maximize -> fullscreen -> unfullscreen
int main() {
    DEFAULT(1);

    // enable auto-tiling
    AWMSG("b r auto_tile");
    sleep(1);

    // get initial workspace state
    AWMSG_J("w l", workspaces);
    json workspace = *workspaces.begin();
    ASSERT(workspace["auto_tile"] == true);

    // maximize the toplevel
    AWMSG("b r maximize");
    sleep(1);

    // verify it's maximized
    AWMSG_J("t l", toplevels);
    json toplevel = *toplevels.begin();
    ASSERT(toplevel["maximized"] == true);

    // fullscreen it (while maximized)
    AWMSG("b r fullscreen");
    sleep(1);

    // verify it's fullscreened
    AWMSG_J("t l", toplevels_fs);
    toplevel = *toplevels_fs.begin();
    ASSERT(toplevel["fullscreen"] == true);

    // unfullscreen it - should return to auto-tile, NOT maximized state
    AWMSG("b r fullscreen");
    sleep(1);

    // verify it's no longer fullscreened
    AWMSG_J("t l", toplevels_unfs);
    toplevel = *toplevels_unfs.begin();
    ASSERT(toplevel["fullscreen"] == false);
    ASSERT(toplevel["maximized"] == false);

    // get output usable area to check if it's being auto-tiled
    AWMSG_J("o l", outputs);
    json output = *outputs.begin();

    // in auto-tile mode with one window, it should fill the usable area
    ASSERT(toplevel["width"] == output["usable"]["width"]);
    ASSERT(toplevel["height"] == output["usable"]["height"]);
    ASSERT(toplevel["x"] == output["usable"]["x"]);
    ASSERT(toplevel["y"] == output["usable"]["y"]);

    EXIT();
}
