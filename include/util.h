#include <stdarg.h>
#include <string>
#include <unistd.h>

#include "wlr.h"

// send a notification
inline void notify_send(const std::string format, ...) {
    // get varidiac
    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format.c_str(), args);
    va_end(args);

    // convert buffer to string
    std::string message = std::string(buffer);

    // log
    wlr_log(WLR_ERROR, "%s", message.c_str());

    // send notification
    if (fork() == 0) {
        execl("/bin/sh", "/bin/sh", "-c",
              ("notify-send -a awm WARNING \"" + std::string(buffer) + "\"")
                  .c_str(),
              nullptr);
    }
}
