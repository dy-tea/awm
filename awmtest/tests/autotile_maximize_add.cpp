#include "../test.h"

// test that maximized toplevels are automatically unmaximized when new
// toplevels are added
// 1. Enable auto-tiling
// 2. Open two toplevels (A and B) - they should tile side by side
// 3. Maximize one (A)
// 4. Open another toplevel (C)
// Expected: A gets automatically unmaximized, and all three (A, B, C) are tiled
// together
int main() {
    DEFAULT(3);

    // enable auto-tiling
    AWMSG("b r auto_tile");
    sleep(1);

    // get initial workspace state
    AWMSG_J("w l", workspaces);
    json workspace = *workspaces.begin();
    ASSERT(workspace["auto_tile"] == true);

    // open first toplevel (already opened by DEFAULT(3))
    sleep(1);

    // verify we have 3 toplevels
    AWMSG_J("t l", toplevels);
    ASSERT(toplevels.size() == 3);

    // get the toplevels
    auto it = toplevels.begin();
    json toplevel_a = *it++;
    json toplevel_b = *it++;
    json toplevel_c = *it++;

    // verify none are maximized initially
    ASSERT(toplevel_a["maximized"] == false);
    ASSERT(toplevel_b["maximized"] == false);
    ASSERT(toplevel_c["maximized"] == false);

    // verify all have reasonable dimensions (should be tiled in a grid)
    // with 3 toplevels in grid mode, they should not take full usable area
    AWMSG_J("o l", outputs);
    json output = *outputs.begin();
    int usable_width = output["usable"]["width"];
    int usable_height = output["usable"]["height"];

    // each toplevel should be smaller than full usable area
    ASSERT(toplevel_a["width"] < usable_width);
    ASSERT(toplevel_b["width"] < usable_width);
    ASSERT(toplevel_c["width"] < usable_width);

    // focus the first toplevel
    AWMSG("b r previous");
    AWMSG("b r previous");
    sleep(1);

    // maximize the first toplevel
    AWMSG("b r maximize");
    sleep(1);

    // verify it's maximized and takes full usable area
    AWMSG_J("t l", toplevels_max);
    auto it_max = toplevels_max.begin();
    json toplevel_a_max = *it_max++;
    json toplevel_b_after = *it_max++;
    json toplevel_c_after = *it_max++;

    ASSERT(toplevel_a_max["maximized"] == true);
    ASSERT(toplevel_a_max["width"] == usable_width);
    ASSERT(toplevel_a_max["height"] == usable_height);

    // the other two should not be maximized
    ASSERT(toplevel_b_after["maximized"] == false);
    ASSERT(toplevel_c_after["maximized"] == false);

    std::cout << "Maximized toplevel verified. Opening new toplevel..."
              << std::endl;

    // open a new toplevel
    spawn("foot");
    sleep(2);

    // verify we have 4 toplevels now
    AWMSG_J("t l", toplevels_final);
    ASSERT(toplevels_final.size() == 4);

    // get all toplevels
    auto it_final = toplevels_final.begin();
    json tl0 = *it_final++;
    json tl1 = *it_final++;
    json tl2 = *it_final++;
    json tl3 = *it_final++;

    // After adding a new toplevel, the previously maximized one should be
    // auto-unmaximized So all 4 toplevels should be tiled (none maximized)
    int maximized_count = 0;

    for (json *tl : {&tl0, &tl1, &tl2, &tl3}) {
        if ((*tl)["maximized"] == true) {
            maximized_count++;
        }
    }

    // there should be NO maximized toplevels after adding a new one
    ASSERT(maximized_count == 0);

    // all 4 toplevels should be tiled (smaller than full usable area)
    for (json *tl : {&tl0, &tl1, &tl2, &tl3}) {
        int width = (*tl)["width"];
        int height = (*tl)["height"];

        // each tiled window should be smaller than full area
        // (they should be in a grid or similar layout)
        ASSERT(width < usable_width || height < usable_height);

        // make sure they're not hidden (width/height should be reasonable)
        ASSERT(width > 0);
        ASSERT(height > 0);
    }

    std::cout << "Test passed: Previously maximized toplevel was "
                 "auto-unmaximized when new toplevel was added"
              << std::endl;

    return 0;
}
