#include <stdarg.h>
#include <string>
#include <unistd.h>

// send a notification
inline void notify_send(const std::string format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format.c_str(), args);
    va_end(args);

    if (fork() == 0) {
        execl("/bin/sh", "/bin/sh", "-c",
              ("notify-send -a awm WARNING \"" + std::string(buffer) + "\"")
                  .c_str(),
              nullptr);
    }
}
