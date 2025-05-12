#include "../test.h"

int main() {
    DEFAULT(3);

    AWMSG("b r next");
    AWMSG("b r next");

    AWMSG("b r window_to 2");
    AWMSG("w s 2");
    AWMSG("b r close");

    AWMSG("w s 1");

    EXIT();
}
