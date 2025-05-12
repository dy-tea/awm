#include "../test.h"

int main() {
    DEFAULT();

    // spawn a toplevel on each workspace
    for (int i = 2; i <= 10; ++i) {
        spawn(terminal_executable);
        sleep(1);

        AWMSG("w s " + std::to_string(i));
    }
    spawn(terminal_executable);
    sleep(1);

    // get toplevels
    AWMSG_J("t l", toplevels);

    // get workspace info
    AWMSG_J("w l", workspaces);

    // assertions
    ASSERT(toplevels.size() == 10);
    ASSERT(workspaces["active"] == 10);
    ASSERT(workspaces["max"] == 10);
    ASSERT(workspaces["toplevels"] == 10);

    EXIT();
}
