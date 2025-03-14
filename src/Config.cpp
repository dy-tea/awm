#include "Server.h"
#include "tomlcpp.hpp"
#include <libinput.h>
#include <sstream>

// get the wlr modifier enum value from the string representation
uint32_t parse_modifier(const std::string &modifier) {

    for (int i = 0; i != 8; ++i)
        if (modifier == MODIFIERS[i])
            return 1 << i;

    return 69420;
}

// create a bind from a space-seperated string of modifiers and key
Bind *parse_bind(const std::string &definition, BindName name) {
    Bind *bind = new Bind{name, 0, 0};

    std::string token;
    std::stringstream ss(definition);

cont:

    // get each space-seperated token
    while (std::getline(ss, token, ' ')) {
        // special handling for number keys
        if (token == "Number") {
            bind->sym = XKB_KEY_NoSymbol;
            continue;
        }

        // mouse buttons
        for (int i = 0; i != 5; ++i)
            if (token == MOUSE_BUTTONS[i]) {
                bind->sym = 0x20000000 + i + 272;
                goto cont; // can't continue here due to for loop
            }

        // parse keysym
        const xkb_keysym_t sym =
            xkb_keysym_from_name(token.c_str(), XKB_KEYSYM_NO_FLAGS);

        if (sym == XKB_KEY_NoSymbol) {
            // not a keysym, parse modifier
            const uint32_t modifier = parse_modifier(token);

            if (modifier == 69420) {
                // not a keysym or modifier, tell user
                notify_send("Config", "No such keycode or modifier '%s'",
                            token.c_str());
                delete bind;
                return nullptr;
            }

            // add modifier
            bind->modifiers |= modifier;
        } else
            // add keysym
            bind->sym = sym;
    }

    return bind;
}

// add bind to binds if valid
void Config::set_bind(const std::string &name, toml::Table *source,
                      const BindName target) {
    if (auto [valid, value] = source->getString(name); valid)
        if (const Bind *bind = parse_bind(value, target)) {
            binds.emplace_back(*bind);
            delete bind;
        }
}

// helper function to connect the second pair if the first bool is true
template <typename T> void connect(const std::pair<bool, T> &pair, T *target) {
    if (pair.first)
        *target = pair.second;
}

Config::Config() {
    path = "";
    last_write_time = std::filesystem::file_time_type::min();
    notify_send("Config", "%s",
                "no config loaded, press Alt+Escape to exit awm");
}

Config::Config(const std::string &path) {
    // path
    this->path = path;

    // get last write time
    last_write_time = std::filesystem::last_write_time(path);

    // load config at path
    load();
}

// load config from path
bool Config::load() {
    // Read in config file
    toml::Result config_file = toml::parseFile(path);

    // false if no config file
    if (!config_file.table) {
        notify_send("Config", "Could not parse config file, %s",
                    config_file.errmsg.c_str());
        return false;
    }

    // Get startup table
    std::unique_ptr<toml::Table> startup =
        config_file.table->getTable("startup");
    if (startup) {
        // startup commands

        if (std::unique_ptr<toml::Array> exec = startup->getArray("exec")) {
            if (auto commands = exec->getStringVector()) {
                // clear commands
                startup_commands.clear();

                for (std::string &command : *commands)
                    // add each one to startup commands
                    startup_commands.emplace_back(command);
            }
        }

        // renderer
        connect(startup->getString("renderer"), &renderer);

        // env vars
        if (std::unique_ptr<toml::Array> env = startup->getArray("env")) {
            if (auto tables = env->getTableVector())
                for (const toml::Table &table : *tables) {
                    std::vector<std::string> keys = table.keys();

                    // clear env vars
                    startup_env.clear();

                    for (const std::string &key : keys) {
                        // try both string and int values
                        auto intval = table.getInt(key);
                        auto stringval = table.getString(key);

                        // add each to startup env
                        if (intval.first)
                            startup_env.emplace_back(
                                key, std::to_string(intval.second));
                        else if (stringval.first)
                            startup_env.emplace_back(key, stringval.second);
                    }
                }
        }
    } else
        wlr_log(WLR_INFO, "%s", "No startup configuration found, ingoring");

    // exit
    std::unique_ptr<toml::Table> exit_table =
        config_file.table->getTable("exit");
    if (exit_table) {
        // list of commands to run on exit
        if (auto exec = exit_table->getArray("exec")) {
            if (auto commands = exec->getStringVector()) {
                // clear exit commands
                exit_commands.clear();

                // add each command to exit commands
                for (std::string &command : *commands)
                    exit_commands.emplace_back(command);
            }
        }
    }

    // ipc
    std::unique_ptr<toml::Table> ipc_table = config_file.table->getTable("ipc");
    if (ipc_table) {
        // enabled
        connect(ipc_table->getBool("enabled"), &ipc.enabled);

        // spawn
        connect(ipc_table->getBool("spawn"), &ipc.spawn);
    }

    // get keyboard config
    std::unique_ptr<toml::Table> keyboard =
        config_file.table->getTable("keyboard");
    if (keyboard) {
        // get layout
        connect(keyboard->getString("layout"), &keyboard_layout);

        // get model
        connect(keyboard->getString("model"), &keyboard_model);

        // get variant
        connect(keyboard->getString("variant"), &keyboard_variant);

        // get options
        connect(keyboard->getString("options"), &keyboard_options);

        // repeat rate
        connect(keyboard->getInt("repeat_rate"), &repeat_rate);

        // repeat delay
        connect(keyboard->getInt("repeat_delay"), &repeat_delay);
    } else
        // no keyboard config
        wlr_log(WLR_INFO, "%s",
                "no keyboard configuration found, using us layout");

    // get pointer config
    std::unique_ptr<toml::Table> pointer =
        config_file.table->getTable("pointer");
    if (pointer) {
        // xcursor
        auto xcursor_table = pointer->getTable("xcursor");
        if (xcursor_table) {
            // theme
            connect(xcursor_table->getString("theme"), &cursor.xcursor.theme);

            // size
            connect(xcursor_table->getInt("size"), &cursor.xcursor.size);
        }

        // used for both mouse and touchpad
        auto pointer_profile = [&](toml::Table *table,
                                   libinput_config_accel_profile *dest,
                                   std::string name) {
            if (auto [fst, snd] = table->getString("profile"); fst) {
                if (snd == "none")
                    *dest = LIBINPUT_CONFIG_ACCEL_PROFILE_NONE;
                else if (snd == "flat")
                    *dest = LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT;
                else if (snd == "adaptive")
                    *dest = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
                else
                    notify_send("Config",
                                "No such option in pointer.%s.profile ['none', "
                                "'flat', 'adaptive']: %s",
                                name.c_str(), snd.c_str());
            }
        };

        // mouse
        std::unique_ptr<toml::Table> mouse = pointer->getTable("mouse");
        if (mouse) {
            // natural scroll
            connect(mouse->getBool("natural_scroll"),
                    &cursor.mouse.natural_scroll);

            // left-handed
            connect(mouse->getInt("left_handed"), &cursor.mouse.left_handed);

            // profile
            pointer_profile(mouse.get(), &cursor.mouse.profile, "mouse");

            // accel speed
            connect(mouse->getDouble("accel_speed"), &cursor.mouse.accel_speed);
        }

        // touchpad
        std::unique_ptr<toml::Table> touchpad = pointer->getTable("touchpad");
        if (touchpad) {
            // tap to click
            auto tap_to_click = touchpad->getBool("tap_to_click");
            if (tap_to_click.first)
                cursor.touchpad.tap_to_click =
                    static_cast<libinput_config_tap_state>(tap_to_click.second);

            // tap and drag
            auto tap_and_drag = touchpad->getBool("tap_and_drag");
            if (tap_and_drag.first)
                cursor.touchpad.tap_and_drag =
                    static_cast<libinput_config_drag_state>(
                        tap_and_drag.second);

            // drag lock
            auto drag_lock = touchpad->getString("drag_lock");
            if (drag_lock.first) {
                if (drag_lock.second == "none")
                    cursor.touchpad.drag_lock =
                        LIBINPUT_CONFIG_DRAG_LOCK_DISABLED;
                else if (drag_lock.second == "timeout")
                    cursor.touchpad.drag_lock =
                        LIBINPUT_CONFIG_DRAG_LOCK_ENABLED_TIMEOUT;
                else if (drag_lock.second == "enabled")
                    cursor.touchpad.drag_lock =
                        LIBINPUT_CONFIG_DRAG_LOCK_ENABLED;
                else if (drag_lock.second == "sticky")
                    cursor.touchpad.drag_lock =
                        LIBINPUT_CONFIG_DRAG_LOCK_ENABLED_STICKY;
                else
                    notify_send(
                        "Config",
                        "No such option in pointer.touchpad.drag_lock ['none', "
                        "'timeout', 'enabled', 'sticky']: %s",
                        drag_lock.second.c_str());
            }

            // tap button map
            auto tap_button_map = touchpad->getString("tap_button_map");
            if (tap_button_map.first) {
                if (tap_button_map.second == "lrm")
                    cursor.touchpad.tap_button_map =
                        LIBINPUT_CONFIG_TAP_MAP_LRM;
                else if (tap_button_map.second == "lmr")
                    cursor.touchpad.tap_button_map =
                        LIBINPUT_CONFIG_TAP_MAP_LMR;
                else
                    notify_send(
                        "Config",
                        "No such option in pointer.tap_button_map ['lrm', "
                        "'lmr']: %s",
                        tap_button_map.second.c_str());
            }

            // natural scroll
            connect(touchpad->getBool("natural_scroll"),
                    &cursor.touchpad.natural_scroll);

            // disable while typing
            auto disable_while_typing =
                touchpad->getBool("disable_while_typing");
            if (disable_while_typing.first)
                cursor.touchpad.disable_while_typing =
                    static_cast<libinput_config_dwt_state>(
                        disable_while_typing.second);

            // left-handed
            connect(touchpad->getInt("left_handed"),
                    &cursor.touchpad.left_handed);

            // middle emulation
            auto middle_emulation = touchpad->getBool("middle_emulation");
            if (middle_emulation.first)
                cursor.touchpad.middle_emulation =
                    static_cast<libinput_config_middle_emulation_state>(
                        middle_emulation.second);

            // scroll method
            auto scroll_method = touchpad->getString("scroll_method");
            if (scroll_method.first) {
                if (scroll_method.second == "none")
                    cursor.touchpad.scroll_method =
                        LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
                else if (scroll_method.second == "2fg")
                    cursor.touchpad.scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;
                else if (scroll_method.second == "edge")
                    cursor.touchpad.scroll_method = LIBINPUT_CONFIG_SCROLL_EDGE;
                else if (scroll_method.second == "button")
                    cursor.touchpad.scroll_method =
                        LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN;
                else
                    notify_send("Config",
                                "No such option in "
                                "pointer.touchpad.scroll_method ['none', "
                                "'2fg', 'edge', 'button']: %s",
                                scroll_method.second.c_str());
            }

            // click method
            auto click_method = touchpad->getString("click_method");
            if (click_method.first) {
                if (click_method.second == "none")
                    cursor.touchpad.click_method =
                        LIBINPUT_CONFIG_CLICK_METHOD_NONE;
                else if (click_method.second == "buttonareas")
                    cursor.touchpad.click_method =
                        LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
                else if (click_method.second == "clickfinger")
                    cursor.touchpad.click_method =
                        LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER;
                else
                    notify_send("Config",
                                "No such option in "
                                "pointer.touchpad.click_method ['none', "
                                "'buttonareas', 'clickfinger']: %s",
                                click_method.second.c_str());
            }

            // event mode
            if (auto [fst, snd] = touchpad->getString("event_mode"); fst) {
                if (snd == "enabled")
                    cursor.touchpad.event_mode =
                        LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
                else if (snd == "disabled")
                    cursor.touchpad.event_mode =
                        LIBINPUT_CONFIG_SEND_EVENTS_DISABLED;
                else if (snd == "mousedisabled")
                    cursor.touchpad.event_mode =
                        LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE;
                else
                    notify_send("Config",
                                "No such option in pointer.touchpad.event_mode "
                                "['enabled', "
                                "'disabled', 'mousedisabled']: %s",
                                snd.c_str());
            }

            // profile
            pointer_profile(touchpad.get(), &cursor.touchpad.profile,
                            "touchpad");

            // accel speed
            connect(touchpad->getDouble("accel_speed"),
                    &cursor.touchpad.accel_speed);
        }
    }

    // get awm binds
    std::unique_ptr<toml::Table> binds_table =
        config_file.table->getTable("binds");
    if (binds_table) {
        // clear binds
        binds.clear();

        // exit bind
        set_bind("exit", binds_table.get(), BIND_EXIT);

        // ensure exit bind is available
        if (binds.empty()) {
            binds.push_back(Bind{BIND_EXIT, WLR_MODIFIER_ALT, XKB_KEY_Escape});
            notify_send("Config", "%s",
                        "No exit bind set, press Alt+Escape to exit awm");
        }

        // window binds
        auto window_bind = binds_table->getTable("window");
        if (window_bind) {
            // window_fullscreen bind
            set_bind("fullscreen", window_bind.get(), BIND_WINDOW_FULLSCREEN);

            // window_previous bind
            set_bind("previous", window_bind.get(), BIND_WINDOW_PREVIOUS);

            // window_next bind
            set_bind("next", window_bind.get(), BIND_WINDOW_NEXT);

            // window_move bind
            set_bind("move", window_bind.get(), BIND_WINDOW_MOVE);

            // window_up bind
            set_bind("up", window_bind.get(), BIND_WINDOW_UP);

            // window_down bind
            set_bind("down", window_bind.get(), BIND_WINDOW_DOWN);

            // window_left bind
            set_bind("left", window_bind.get(), BIND_WINDOW_LEFT);

            // window_right bind
            set_bind("right", window_bind.get(), BIND_WINDOW_RIGHT);

            // window_close bind
            set_bind("close", window_bind.get(), BIND_WINDOW_CLOSE);

            auto swap_bind = window_bind->getTable("swap");
            if (swap_bind) {
                // window_swap_up bind
                set_bind("up", swap_bind.get(), BIND_WINDOW_SWAP_UP);

                // window_swap_down bind
                set_bind("down", swap_bind.get(), BIND_WINDOW_SWAP_DOWN);

                // window_swap_left bind
                set_bind("left", swap_bind.get(), BIND_WINDOW_SWAP_LEFT);

                // window_swap_right bind
                set_bind("right", swap_bind.get(), BIND_WINDOW_SWAP_RIGHT);
            }
        }

        // workspace binds
        auto workspace_bind = binds_table->getTable("workspace");
        if (workspace_bind) {
            // workspace_tile bind
            set_bind("tile", workspace_bind.get(), BIND_WORKSPACE_TILE);

            // workspace_open bind
            set_bind("open", workspace_bind.get(), BIND_WORKSPACE_OPEN);

            // workspace_window_to bind
            set_bind("window_to", workspace_bind.get(),
                     BIND_WORKSPACE_WINDOW_TO);
        }
    }

    // Get user-defined commands
    std::unique_ptr<toml::Array> command_tables =
        config_file.table->getArray("commands");
    if (command_tables) {
        auto tables = command_tables->getTableVector();

        // clear commands
        commands.clear();

        if (tables)
            for (toml::Table &table : *tables) {
                auto bind = table.getString("bind");
                auto exec = table.getString("exec");

                if (bind.first && exec.first)
                    if (Bind *parsed = parse_bind(bind.second, BIND_NONE)) {
                        commands.emplace_back(*parsed, exec.second);
                        delete parsed;
                    }
            }
    } else
        wlr_log(WLR_INFO, "%s", "No user-defined commands set, ignoring");

    // monitor configs
    std::unique_ptr<toml::Array> monitor_tables =
        config_file.table->getArray("monitors");
    if (monitor_tables) {
        if (auto tables = monitor_tables->getTableVector())
            for (toml::Table &table : *tables) {
                // create new output config
                auto *oc = new OutputConfig();

                // name
                connect(table.getString("name"), &oc->name);

                // enabled
                connect(table.getBool("enabled"), &oc->enabled);

                // width
                connect<int32_t>(table.getInt("width"), &oc->width);

                // height
                connect<int32_t>(table.getInt("height"), &oc->height);

                // x
                connect(table.getDouble("x"), &oc->x);

                // y
                connect(table.getDouble("y"), &oc->y);

                // refresh rate
                connect(table.getDouble("refresh"), &oc->refresh);

                // transform
                auto transform = table.getString("transform");
                std::string transform_values[] = {
                    "none", "90", "180", "270", "f", "f90", "f180", "f270"};
                if (transform.first)
                    for (int i = 0; i != WL_OUTPUT_TRANSFORM_FLIPPED_270 + 1;
                         ++i)
                        if (transform.second == transform_values[i]) {
                            oc->transform = static_cast<wl_output_transform>(i);
                            break;
                        }

                // scale
                connect<float>(table.getDouble("scale"), &oc->scale);

                // adaptive sync
                connect(table.getBool("adaptive"), &oc->adaptive_sync);

                // add to output configs if enough values are set
                if (oc->name.empty() || !oc->width || !oc->height ||
                    oc->refresh <= 0.0) {
                    notify_send("Config", "%s",
                                "monitor config is missing one of the required "
                                "fields: name, width, height, refresh");
                    delete oc;
                } else {
                    wlr_log(WLR_INFO, "added monitor config for %s: %dx%d@%.1f",
                            oc->name.c_str(), oc->width, oc->height,
                            oc->refresh);
                    outputs.emplace_back(oc);
                }
            }
    }

    return true;
}

// update the config
void Config::update(const Server *server) {
    // ignore updates on empty path
    if (path.empty())
        return;

    // get current write time
    const std::filesystem::file_time_type current_write_time =
        std::filesystem::last_write_time(path);

    // check if the file has been modified
    if (current_write_time == last_write_time)
        return;

    // update write time
    last_write_time = current_write_time;

    // load new config
    if (!load())
        return;

    // update keyboard config
    const wlr_keyboard *wlr_keyboard = wlr_seat_get_keyboard(server->seat);
    const auto *keyboard = static_cast<Keyboard *>(wlr_keyboard->data);
    keyboard->update_config();

    // update cursor config
    server->cursor->reconfigure_all();

    // notify user of reload
    notify_send("Config", "%s", "config reload complete");
}
