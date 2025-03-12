#include <memory>
#include <stdexcept>
#include <string>
#include <unistd.h>

#include "wlr.h"

// send a notification
template <typename... Args>
void notify_send(const std::string title, const std::string &format,
                 Args... args) {
    // format message
    const int size_s = std::snprintf(nullptr, 0, format.c_str(), args...) + 1;
    if (size_s <= 0)
        throw std::runtime_error("Error during formatting.");
    const auto size = static_cast<size_t>(size_s);
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    const std::string message = std::string(buf.get(), buf.get() + size - 1);

    // log
    wlr_log(WLR_INFO, "%s", message.c_str());

    // send notification
    if (fork() == 0) {
        execl("/bin/sh", "/bin/sh", "-c",
              ("notify-send -a awm " + title + "\"" + message + "\"").c_str(),
              nullptr);
    }
}

// stolen from https://stackoverflow.com/a/26221725
template <typename... Args>
std::string string_format(const std::string &format, Args... args) {
    int size_s = std::snprintf(nullptr, 0, format.c_str(), args...) + 1;
    if (size_s <= 0)
        throw std::runtime_error("Error during formatting.");
    auto size = static_cast<size_t>(size_s);
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1);
}
