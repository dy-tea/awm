#include "wlr.h"
#include <iostream>
#include <vector>

struct Bind {
    uint32_t modifers;
    xkb_keysym_t sym;
};

struct Config {
    std::vector<std::string> startup_commands;
    std::vector<std::pair<std::string, std::string>> startup_env;
    std::vector<Bind> binds;

    Config(std::string path);
    ~Config();
};
