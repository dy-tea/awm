#include "../test.h"

// fullscreen 10 times, the toplevel geometry should be the same
int main() {
    DEFAULT(1);

    AWMSG_J("t l", toplevels);

    // fullscreen 10 times
    for (int i = 0; i != 10; i++)
        AWMSG("b r fullscreen");

    AWMSG_J("t l", toplevels2);

    ASSERT(toplevels == toplevels2);

    EXIT();
}
