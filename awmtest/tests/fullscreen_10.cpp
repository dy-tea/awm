#include "../test.h"

// fullscreen 10 times, the toplevel geometry should be the same
int main() {
    DEFAULT(1);

    json toplevels = awmsg("t l", true);

    std::cout << toplevels.dump(4) << std::endl;

    // fullscreen 10 times
    for (int i = 0; i != 10; i++) {
        sleep(1);
        awmsg("b r fullscreen", false);
    }

    sleep(1);

    json toplevels2 = awmsg("t l", true);

    std::cout << toplevels2.dump(4) << std::endl;

    ASSERT(toplevels == toplevels2);

    EXIT();
}
