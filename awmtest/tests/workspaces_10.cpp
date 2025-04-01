#include "../test.h"

int main() {
    DEFAULT();

    // spawn a toplevel on each workspace
    for (int i = 2; i <= 10; ++i) {
        spawn(terminal_executable);
        sleep(1);

        awmsg("w s " + std::to_string(i), false);
        sleep(1);
    }
    spawn(terminal_executable);
    sleep(1);

    // get toplevels
    json toplevels = awmsg("t l", true);
    sleep(1);

    // get workspace info
    json workspaces = awmsg("w l", true);
    sleep(1);

    // assertions
    ASSERT(toplevels.size() == 10);
    ASSERT(workspaces["active"] == 10);
    ASSERT(workspaces["max"] == 10);
    ASSERT(workspaces["toplevels"] == 10);

    EXIT();
}
