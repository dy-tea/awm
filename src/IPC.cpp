#include "Server.h"
#include <string>
#include <sys/socket.h>
using json = nlohmann::json;

IPC::IPC(Server *server, std::string sock_path)
    : server(server), path(sock_path) {
    // create file descriptor
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        wlr_log(WLR_ERROR, "%s", "failed to create IPC socket");
        return;
    }

    // find an available socket path
    if (path.empty())
        for (int i = 0; i != 255; ++i) {
            std::string current_path =
                "/tmp/awm-" + std::to_string(i) + ".sock";
            if (access(current_path.c_str(), F_OK)) {
                path = current_path;
                break;
            }
        }

    // no path found
    if (path.empty()) {
        wlr_log(WLR_ERROR, "%s",
                "failed to find an available IPC socket path, clear your /tmp "
                "or something");
        return;
    }

    wlr_log(WLR_INFO, "starting IPC with fd %d on path `%s`", fd, path.c_str());

    // set socket address
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

    // set environment variable
    setenv("AWM_SOCKET", addr.sun_path, 1);

    // bind socket
    if (bind(fd, reinterpret_cast<struct sockaddr *>(&addr),
             sizeof(struct sockaddr_un)) == -1) {
        wlr_log(WLR_ERROR, "failed to bind socket with fd `%d` on path `%s`",
                fd, path.c_str());
        return;
    }

    // listen for connections
    if (listen(fd, 128) == -1) {
        wlr_log(WLR_ERROR,
                "failed to listen on socket with fd `%d` on path `%s`", fd,
                path.c_str());
        return;
    }

    // start thread
    // FIXME: add to event loop
    thread = std::thread([&]() {
        while (running) {
            // accept connections
            int client_fd = accept(fd, nullptr, nullptr);
            if (client_fd == -1) {
                wlr_log(WLR_ERROR,
                        "failed to accept connection on socket with fd `%d` on "
                        "path `%s`",
                        fd, path.c_str());
                continue;
            }

            // read from client
            char buffer[1024];
            int len = read(client_fd, buffer, sizeof(buffer));
            if (len == -1) {
                wlr_log(WLR_ERROR,
                        "failed to read from client with fd `%d` on path `%s`",
                        client_fd, path.c_str());
                close(client_fd);
                continue;
            }

            // parse client message
            std::string message(buffer, len);
            bool continuous = false;

            // parse continuous command
            if (message[0] == 'c') {
                continuous = true;
                message = message.substr(2);
            }

            // run command
            std::string response =
                parse_command(message, client_fd, continuous);

            // write response to client
            if (write(client_fd, response.c_str(), strlen(response.c_str())) ==
                -1)
                wlr_log(WLR_ERROR,
                        "failed to write to client with fd `%d` on path `%s`",
                        client_fd, path.c_str());

            // close connection
            if (!continuous)
                close(client_fd);
        }
    });
}

// parse a command and return the response
std::string IPC::parse_command(const std::string &command, const int client_fd,
                               const bool continuous) {
    std::string token, data, tmp;
    IPCMessage message{IPC_NONE};

    std::stringstream ss(command);

    wlr_log(WLR_INFO, "received command `%s`", command.c_str());

    if (std::getline(ss, token, ' ')) {
        switch (token[0]) {
        case 'e': // exit
            message = IPC_EXIT;
            break;
        case 's': // spawn
            while (std::getline(ss, tmp, ' ')) {
                data += tmp + " ";
                message = IPC_SPAWN;
            }
            if (!data.empty())
                break;
            goto unknown;
        case 'o': // output
            if (std::getline(ss, token, ' ')) {
                if (token[0] == 'l') { // output list
                    message = IPC_OUTPUT_LIST;
                    break;
                } else if (token[0] == 'm') { // output modes
                    message = IPC_OUTPUT_MODES;
                    break;
                }
            }
            goto unknown;
        case 'w': // workspace
            if (std::getline(ss, token, ' ')) {
                if (token[0] == 'l') { // workspace list
                    message = IPC_WORKSPACE_LIST;
                    break;
                } else if (token[0] == 's') // workspace set
                    if (std::getline(ss, token, ' ')) {
                        data = token;
                        message = IPC_WORKSPACE_SET;
                        break;
                    }
            }
            goto unknown;
        case 't': // toplevel
            if (std::getline(ss, token, ' '))
                if (token[0] == 'l') { // toplevel list
                    message = IPC_TOPLEVEL_LIST;
                    break;
                }
            goto unknown;
        case 'k': // keyboard
            if (std::getline(ss, token, ' '))
                if (token[0] == 'l') { // keyboard list
                    message = IPC_KEYBOARD_LIST;
                    break;
                }
            goto unknown;
        case 'd': // device
            if (std::getline(ss, token, ' ')) {
                if (token[0] == 'l') { // device list
                    message = IPC_DEVICE_LIST;
                    break;
                } else if (token[0] == 'c') { // device current
                    message = IPC_DEVICE_CURRENT;
                    break;
                }
            }
            goto unknown;
        case 'b': // bind
            if (std::getline(ss, token, ' ')) {
                if (token[0] == 'l') { // bind list
                    message = IPC_BIND_LIST;
                    break;
                } else if (token[0] == 'r') { // bind run
                    if (std::getline(ss, token, ' ')) {
                        data = token;
                        message = IPC_BIND_RUN;
                        break;
                    }
                } else if (token[0] == 'd') // bind display
                    if (std::getline(ss, token, ' ')) {
                        data = token;
                        message = IPC_BIND_DISPLAY;
                        break;
                    }
            }
            [[fallthrough]];
        default:
        unknown:
            notify_send("IPC", "unknown command `%s`", token.c_str());
        }
    } else
        notify_send("IPC", "received empty command");

    // subscribe to command if continuous
    if (continuous && message != IPC_NONE) {
        wlr_log(WLR_INFO, "subscribed client with fd %d", client_fd);
        std::lock_guard<std::mutex> lock(subscriptions_mutex);
        subscriptions[client_fd].push_back({message, data});
    }

    // return parsed command
    return handle_command(message, data).dump();
}

// handle an IPC message, return the response json
json IPC::handle_command(const IPCMessage message, const std::string &data) {
    json j;

    switch (message) {
    case IPC_EXIT:
        server->exit();
        break;
    case IPC_SPAWN:
        if (server->config->ipc.spawn)
            server->spawn(data);
        break;
    case IPC_OUTPUT_LIST: {
        Output *output, *tmp;
        wl_list_for_each_safe(output, tmp, &server->output_manager->outputs,
                              link) {
            // outputs are distinguished by name
            j[output->wlr_output->name] = {
                {"layout",
                 {
                     {"x", output->layout_geometry.x},
                     {"y", output->layout_geometry.y},
                     {"width", output->layout_geometry.width},
                     {"height", output->layout_geometry.height},
                 }},
                {"usable",
                 {
                     {"x", output->usable_area.x},
                     {"y", output->usable_area.y},
                     {"width", output->usable_area.width},
                     {"height", output->usable_area.height},
                 }},
                {"refresh", output->wlr_output->refresh / 1000.0},
                {"scale", output->wlr_output->scale},
                {"transform", output->wlr_output->transform},
                {"adaptive", output->wlr_output->adaptive_sync_supported},
                {"enabled", output->wlr_output->enabled},
                {"focused", output == server->focused_output()},
                {"workspace", output->get_active()->num},
            };

            // below values may be null (e.g. virtual outputs)
            // they are generally set on physical displays though
            if (output->wlr_output->description)
                j[output->wlr_output->name]["description"] =
                    output->wlr_output->description;

            if (output->wlr_output->make)
                j[output->wlr_output->name]["make"] = output->wlr_output->make;

            if (output->wlr_output->model)
                j[output->wlr_output->name]["model"] =
                    output->wlr_output->model;

            if (output->wlr_output->serial)
                j[output->wlr_output->name]["serial"] =
                    output->wlr_output->serial;
        }
        break;
    }
    case IPC_OUTPUT_MODES: {
        Output *output, *tmp;
        wl_list_for_each_safe(output, tmp, &server->output_manager->outputs,
                              link) {
            wlr_output_mode *mode, *tmp1;
            int i = 0;
            wl_list_for_each_safe(mode, tmp1, &output->wlr_output->modes,
                                  link) {
                // output modes are defined as a size, refresh rate and
                // preferred status - the selected mode is the current mode
                j[output->wlr_output->name][i++] = {
                    {"refresh", mode->refresh / 1000.0},
                    {"width", mode->width},
                    {"height", mode->height},
                    {"preferred", mode->preferred},
                    {"selected", mode == output->wlr_output->current_mode},
                };
            }
        }

        break;
    }
    case IPC_WORKSPACE_LIST: {
        Output *output = server->focused_output();

        // max output post-increments so we subtract 1
        j["max"] = output->max_workspace - 1;

        // get active workspace
        j["active"] = output->get_active()->num;

        // find the active workspace and count toplevels
        int toplevel_count = 0;
        Workspace *workspace, *tmp;
        wl_list_for_each_safe(workspace, tmp, &output->workspaces, link)
            toplevel_count += wl_list_length(&workspace->toplevels);

        j["toplevels"] = toplevel_count;
        break;
    }
    case IPC_WORKSPACE_SET: {
        try {
            // try parsing an integer
            const uint32_t n = std::stoi(data);

            // integer should be below max workspace, since it is always
            // workspace count + 1
            if (n > server->focused_output()->max_workspace)
                throw std::invalid_argument("out of range");

            // set workspace
            server->focused_output()->set_workspace(n);
        } catch (std::invalid_argument &e) {
            // if your script is incorrect it is better to notify the user
            notify_send("IPC", "invalid workspace number `%s`: %s",
                        data.c_str(), e.what());
        }

        // we do not set any json
        break;
    }
    case IPC_TOPLEVEL_LIST: {
        // list all toplevels regardless of workspace
        Output *o, *t0;
        Workspace *w, *t1;
        Toplevel *t, *t2;

        wl_list_for_each_safe(o, t0, &server->output_manager->outputs, link)
            wl_list_for_each_safe(w, t1, &o->workspaces, link)
                wl_list_for_each_safe(t, t2, &w->toplevels, link)
            // toplevels are indexed by their pointer as title is
            // non-unique
            j[string_format("%p", t)] = {
                {"title", t->title()},
                {"x", t->geometry.x},
                {"y", t->geometry.y},
                {"width", t->geometry.width},
                {"height", t->geometry.height},
                {"focused", t == w->active_toplevel},
                {"hidden", t->hidden},
                {"maximized", t->maximized()},
                {"fullscreen", t->fullscreen()},
#ifdef XWAYLAND
                {"xwayland", !t->xdg_toplevel},
#endif
            };

        break;
    }
    case IPC_KEYBOARD_LIST: {
        // just return the keyboard config
        j = {{"layout", server->config->keyboard_layout},
             {"model", server->config->keyboard_model},
             {"variant", server->config->keyboard_variant},
             {"options", server->config->keyboard_options},
             {"repeat_rate", server->config->repeat_rate},
             {"repeat_delay", server->config->repeat_delay}};
        break;
    }
    case IPC_DEVICE_LIST: {
        Keyboard *keyboard, *tmp;
        int i = 0;
        wl_list_for_each_safe(keyboard, tmp, &server->keyboards, link) {
            // get device type
            std::string type = "";
            switch (keyboard->wlr_keyboard->base.type) {
            case WLR_INPUT_DEVICE_KEYBOARD:
                type = "keyboard";
                break;
            case WLR_INPUT_DEVICE_POINTER:
                type = "pointer";
                break;
            case WLR_INPUT_DEVICE_TOUCH:
                type = "touch";
                break;
            case WLR_INPUT_DEVICE_TABLET:
                type = "tablet";
                break;
            case WLR_INPUT_DEVICE_TABLET_PAD:
                type = "tablet_pad";
                break;
            case WLR_INPUT_DEVICE_SWITCH:
                type = "switch";
                break;
            default:
                break;
            }

            // add to list of devices
            j[i++] = {
                {"name", keyboard->wlr_keyboard->base.name},
                {"type", type},
            };
        }

        break;
    }
    case IPC_DEVICE_CURRENT: {
        // get the current device layout
        Keyboard *keyboard, *tmp;
        wl_list_for_each_safe(keyboard, tmp, &server->keyboards, link) {
            // keyboards can have multiple layouts
            xkb_keymap *keymap = keyboard->wlr_keyboard->keymap;
            xkb_state *state = keyboard->wlr_keyboard->xkb_state;
            xkb_layout_index_t num_layouts =
                keymap ? xkb_keymap_num_layouts(keymap) : 0;

            for (xkb_layout_index_t layout_idx = 0; layout_idx < num_layouts;
                 layout_idx++) {

                // this long name is nicer for clients
                std::string layout_name =
                    xkb_keymap_layout_get_name(keymap, layout_idx);

                // can be -1 if invalid
                int layout_enabled = xkb_state_layout_index_is_active(
                    state, layout_idx, XKB_STATE_LAYOUT_EFFECTIVE);

                j[keyboard->wlr_keyboard->base.name
                      ? keyboard->wlr_keyboard->base.name
                      : "default"][layout_idx] = {
                    {"enabled", layout_enabled},
                    {"layout", layout_name},
                };
            }
        }

        break;
    }
    case IPC_BIND_LIST: {
        int i = 0;
        for (const Bind &bind : server->config->binds) {
            // get modifier string
            std::string modifiers = "";
            for (int j = 0; j != 8; ++j)
                if (bind.modifiers & 1 << j)
                    modifiers += MODIFIERS[j] + " ";

            // get the name of the keysym
            char buffer[255];
            xkb_keysym_get_name(bind.sym, buffer, 255);
            std::string name(buffer);

            // handle mouse binds
            if (bind.sym >= 0x20000000 + 272 && bind.sym <= 0x20000000 + 276)
                name = MOUSE_BUTTONS[bind.sym - 0x20000000 - 272];

            // add to list of binds
            j[i++] = {
                {"name", BIND_NAMES[bind.name]},
                {"modifiers", modifiers.empty()
                                  ? "None"
                                  : modifiers.substr(0, modifiers.size() - 1)},
                {"sym", bind.sym == XKB_KEY_NoSymbol ? "Number" : name},
            };
        }
        break;
    }
    case IPC_BIND_RUN: {
        for (unsigned long i = 0; i != BIND_NAMES->length(); ++i) {
            if (data == BIND_NAMES[i]) {
                server->handle_bind(Bind{static_cast<BindName>(i), 0, 0});
                break;
            }
        }
        break;
    }
    case IPC_BIND_DISPLAY: {
        for (unsigned long i = 0; i != BIND_NAMES->length(); ++i) {
            if (data == BIND_NAMES[i]) {
                // get the bind
                Bind bind{};
                for (Bind b : server->config->binds)
                    if (b.name == i) {
                        bind = b;
                        break;
                    }

                // get modifier string
                std::string modifiers = "";
                for (int j = 0; j != 8; ++j)
                    if (bind.modifiers & 1 << j)
                        modifiers += MODIFIERS[j] + " ";

                // get the name of the keysym
                std::string name;
                xkb_keysym_get_name(bind.sym, name.data(), sizeof(name));

                // display
                j = {
                    {"name", BIND_NAMES[i]},
                    {"modifiers",
                     modifiers.empty()
                         ? "None"
                         : modifiers.substr(0, modifiers.size() - 1)},
                    {"sym", bind.sym == XKB_KEY_NoSymbol ? "Number" : name},
                };
                break;
            }
        }
        break;
    }
    case IPC_NONE:
    default:
        break;
    }

    return j;
}

// notify clients of a message
void IPC::notify_clients(const IPCMessage message) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex);

    // iterate through each subscribed client
    for (auto it = subscriptions.begin(); it != subscriptions.end();) {
        int client_fd = it->first;
        const auto &messages = it->second;

        // write to client if subscribed to message
        bool disconnected = false;
        for (const auto &m : messages) {
            if (m.first == message) {
                // get new data
                json j = handle_command(message, m.second);
                std::string data = j.dump();

                wlr_log(WLR_INFO, "notifying client `%d` of message `%d`",
                        it->first, message);

                // write to client
                if (send(client_fd, data.c_str(), data.size(), MSG_NOSIGNAL) ==
                    -1) {
                    // close connection and remove subscription if write
                    // failed
                    wlr_log(WLR_ERROR,
                            "failed to write to client with fd `%d` on "
                            "path `%s`, closing connection",
                            client_fd, path.c_str());
                    close(client_fd);
                    disconnected = true;
                }

                // a client should not receive the same message twice
                break;
            }
        }

        // remove client if disconnected
        if (disconnected)
            it = subscriptions.erase(it);
        else
            ++it;
    }
}

void IPC::notify_clients(const std::vector<IPCMessage> &messages) {
    for (const IPCMessage &message : messages)
        notify_clients(message);
}

void IPC::stop() {
    // close
    close(fd);

    // unlink
    if (unlink(path.c_str()))
        wlr_log(WLR_ERROR, "failed to unlink IPC socket at path `%s`",
                path.c_str());

    // stop thread
    running = false;

    pthread_cancel(thread.native_handle());
    if (thread.joinable())
        thread.join();

    delete this;
}
