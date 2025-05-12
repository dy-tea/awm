#include "../test.h"

int main() {
    DEFAULT(3);

    awmsg("b r next", false);
    sleep(1);

    awmsg("b r next", false);
    sleep(1);

    awmsg("b r window_to 2", false);
    sleep(1);

    awmsg("w s 2", false);
    sleep(1);

    awmsg("b r close", false);
    sleep(1);

    if (awmsg("w s 1", true) == json())
        return 1;
    sleep(1);

    EXIT();
}
