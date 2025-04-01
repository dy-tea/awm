#include "../test.h"

// maximize 10 times, the toplevel geometry should be the same
int main() {
    DEFAULT(1);

    // get toplevel bounds
    json toplevel = awmsg("t l", true);

    // send maximize 10 times
    for (int i = 0; i != 10; ++i) {
        sleep(1);
        awmsg("b r maximize", false);
    }

    sleep(1);

    // get toplevel bounds
    json toplevel1 = awmsg("t l", true);

    ASSERT(toplevel == toplevel1);

    EXIT();
}
