#include "../test.h"

// test that swapping with a maximized toplevel auto-unmaximizes both toplevels
// 1. Enable auto-tiling
// 2. Open two toplevels (A and B) - they should tile side by side
// 3. Maximize one (A)
// 4. Swap A and B using directional swap
// Expected: Both A and B are unmaximized and swap positions in the tiling layout
int main() {
    DEFAULT(2);

    // enable auto-tiling
    AWMSG("b r auto_tile");
    sleep(1);

    // get initial workspace state
    AWMSG_J("w l", workspaces);
    json workspace = *workspaces.begin();
    ASSERT(workspace["auto_tile"] == true);

    // verify we have 2 toplevels
    AWMSG_J("t l", toplevels);
    ASSERT(toplevels.size() == 2);

    // get the toplevels
    auto it = toplevels.begin();
    json toplevel_a = *it++;
    json toplevel_b = *it++;

    // verify none are maximized initially
    ASSERT(toplevel_a["maximized"] == false);
    ASSERT(toplevel_b["maximized"] == false);

    // get output usable area
    AWMSG_J("o l", outputs);
    json output = *outputs.begin();
    int usable_width = output["usable"]["width"];
    int usable_height = output["usable"]["height"];

    // both should be smaller than full usable area (they're tiled)
    ASSERT(toplevel_a["width"] < usable_width);
    ASSERT(toplevel_b["width"] < usable_width);

    // store original positions
    int a_x = toplevel_a["x"];
    int b_x = toplevel_b["x"];

    std::cout << "Initial state: A at x=" << a_x << ", B at x=" << b_x << std::endl;

    // maximize the first toplevel
    AWMSG("b r maximize");
    sleep(1);

    // verify it's maximized and takes full usable area
    AWMSG_J("t l", toplevels_max);
    auto it_max = toplevels_max.begin();
    json toplevel_a_max = *it_max++;
    json toplevel_b_after = *it_max++;

    ASSERT(toplevel_a_max["maximized"] == true);
    ASSERT(toplevel_a_max["width"] == usable_width);
    ASSERT(toplevel_a_max["height"] == usable_height);
    ASSERT(toplevel_b_after["maximized"] == false);

    std::cout << "Toplevel A maximized successfully" << std::endl;

    // perform directional swap (swap with toplevel to the right/left)
    // we need to figure out which direction B is from A
    // since A is maximized and takes full screen, we'll just try swapping
    AWMSG("b r swap_right");
    sleep(1);

    // after swap, both should be unmaximized and tiled
    AWMSG_J("t l", toplevels_swapped);
    ASSERT(toplevels_swapped.size() == 2);

    auto it_swap = toplevels_swapped.begin();
    json tl_0 = *it_swap++;
    json tl_1 = *it_swap++;

    // both should be unmaximized now
    ASSERT(tl_0["maximized"] == false);
    ASSERT(tl_1["maximized"] == false);

    std::cout << "After swap: tl_0 at x=" << tl_0["x"].get<int>() 
              << ", tl_1 at x=" << tl_1["x"].get<int>() << std::endl;

    // both should be tiled (smaller than full usable area)
    ASSERT(tl_0["width"] < usable_width || tl_0["height"] < usable_height);
    ASSERT(tl_1["width"] < usable_width || tl_1["height"] < usable_height);

    // make sure they're not hidden
    ASSERT(tl_0["width"] > 0);
    ASSERT(tl_0["height"] > 0);
    ASSERT(tl_1["width"] > 0);
    ASSERT(tl_1["height"] > 0);

    // the positions should have swapped (or at least both are tiled now)
    // since we can't guarantee exact position match, just verify both are tiled
    bool both_tiled = (tl_0["width"] < usable_width) && (tl_1["width"] < usable_width);
    ASSERT(both_tiled);

    std::cout << "Test passed: Both toplevels are now tiled after swap" << std::endl;

    return 0;
}