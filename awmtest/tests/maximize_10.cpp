#include "../test.h"

// maximize 10 times, the toplevel geometry should be the same
int main() {
    DEFAULT(1);

    // get toplevel bounds
    AWMSG_J("t l", toplevel);

    // send maximize 10 times
    for (int i = 0; i != 10; ++i)
        AWMSG("b r maximize");

    // get toplevel bounds
    AWMSG_J("t l", toplevel1);

    ASSERT(toplevel == toplevel1);

    EXIT();
}
