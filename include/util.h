#include <memory>
#include <stdarg.h>
#include <stdexcept>
#include <string>
#include <unistd.h>

#include "wlr.h"

// send a notification
inline void notify_send(const std::string &format, ...) {
    // get variadic
    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format.c_str(), args);
    va_end(args);

    // convert buffer to string
    const std::string message = std::string(buffer);

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

template <typename... Args>
std::string string_format(const std::string &format, Args... args) {
    int size_s = std::snprintf(nullptr, 0, format.c_str(), args...) + 1;
    if (size_s <= 0) {
        throw std::runtime_error("Error during formatting.");
    }
    auto size = static_cast<size_t>(size_s);
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1);
}
