#include "IPC.h"
#include "Config.h"
#include "Keyboard.h"
#include "OutputManager.h"
#include "Server.h"
#include "Toplevel.h"
#include "WorkspaceManager.h"
#include "util.h"
#include <string>
#include <sys/socket.h>
using json = nlohmann::ordered_json;

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

    // add to event loop
    source = wl_event_loop_add_fd(
        wl_display_get_event_loop(server->display), fd, WL_EVENT_READABLE,
        +[](int fd, [[maybe_unused]] unsigned int mask, void *data) {
            IPC *ipc = static_cast<IPC *>(data);

            // accept connections
            int client_fd = accept(fd, nullptr, nullptr);
            if (client_fd == -1) {
                wlr_log(WLR_ERROR,
                        "failed to accept connection on socket with fd `%d` on "
                        "path `%s`",
                        fd, ipc->path.c_str());
                return 0;
            }

            // read from client
            char buffer[1024];
            int len = read(client_fd, buffer, sizeof(buffer));
            if (len == -1) {
                wlr_log(WLR_ERROR,
                        "failed to read from client with fd `%d` on path `%s`",
                        client_fd, ipc->path.c_str());
                close(client_fd);
                return 0;
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
                ipc->parse_command(message, client_fd, continuous);

            // write response to client
            if (write(client_fd, response.c_str(), strlen(response.c_str())) ==
                -1)
                wlr_log(WLR_ERROR,
                        "failed to write to client with fd `%d` on path `%s`",
                        client_fd, ipc->path.c_str());

            // close connection
            if (!continuous)
                close(client_fd);
            return 0;
        },
        this);
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
                switch (token[0]) {
                case 'l': // output list
                    message = IPC_OUTPUT_LIST;
                    break;
                case 't': // output toplevels
                    message = IPC_OUTPUT_TOPLEVELS;
                    break;
                case 'm': // output modes
                    message = IPC_OUTPUT_MODES;
                    break;
                default:
                    goto unknown;
                }
                break;
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
                switch (token[0]) {
                case 'l': // bind list
                    message = IPC_BIND_LIST;
                    break;
                case 'r': // bind run
                    if (std::getline(ss, token, ' ')) {
                        data = token;
                        message = IPC_BIND_RUN;

                        // some binds have extra data
                        if (std::getline(ss, token, ' '))
                            data += "," + token;

                        break;
                    }
                    goto unknown;
                case 'd': // bind display
                    if (std::getline(ss, token, ' ')) {
                        data = token;
                        message = IPC_BIND_DISPLAY;
                        break;
                    }
                default:
                    goto unknown;
                }
                break;
            }
            goto unknown;
        case 'r': // windowrule
            if (std::getline(ss, token, ' ')) {
                switch (token[0]) {
                case 'l': // windowrule list
                    message = IPC_RULE_LIST;
                    break;
                default:
                    goto unknown;
                }
                break;
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
            // shorter for convenience
            wlr_output *o = output->wlr_output;

            std::function<std::string(uint32_t)> reverse_fourcc =
                [](uint32_t fourcc) {
                    return std::string{
                        static_cast<char>(fourcc & 0xff),
                        static_cast<char>((fourcc >> 8) & 0xff),
                        static_cast<char>((fourcc >> 16) & 0xff),
                        static_cast<char>((fourcc >> 24) & 0xff)};
                };

            auto format_binary = [](uint32_t value) {
                return std::bitset<32>(value).to_string();
            };

            // outputs are distinguished by name
            j[o->name] = {
                {"enabled", o->enabled},
                {"focused", output == server->focused_output()},
                {"workspace", output->get_active()->num},
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
                {"refresh", o->refresh / 1000.0},
                {"scale", o->scale},
                {"transform", o->transform},
                {"adaptive_sync_supported", o->adaptive_sync_supported},
                {"render_format", reverse_fourcc(o->render_format)},
                {"supported_primaries", format_binary(o->supported_primaries)},
                {"supported_transfer_functions",
                 format_binary(o->supported_transfer_functions)}};

            // below values may be null (e.g. virtual outputs)
            // they are generally set on physical displays though
            if (o->description)
                j[o->name]["description"] = o->description;

            if (o->make)
                j[o->name]["make"] = o->make;

            if (o->model)
                j[o->name]["model"] = o->model;

            if (o->serial)
                j[o->name]["serial"] = o->serial;
        }
        break;
    }
    case IPC_OUTPUT_TOPLEVELS: {
        Workspace *workspace, *tmp1;
        Toplevel *toplevel, *tmp2;
        wl_list_for_each_safe(workspace, tmp1,
                              &server->workspace_manager->workspaces, link) {
            int i = 0;
            j[workspace->output->wlr_output->name][workspace->num] =
                json::array();
            wl_list_for_each_safe(toplevel, tmp2, &workspace->toplevels, link)
                j[workspace->output->wlr_output->name][workspace->num - 1]
                 [i++] = string_format("%p", toplevel);
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
        WorkspaceManager *manager = server->workspace_manager;

        // count toplevels
        int toplevel_count = 0;
        Workspace *workspace, *tmp;
        wl_list_for_each_safe(workspace, tmp, &manager->workspaces, link)
            toplevel_count += wl_list_length(&workspace->toplevels);
        j["toplevels"] = toplevel_count;

        // find focused output and active workspace
        Output *focused = server->focused_output();
        j["focused_output"] = focused->wlr_output->name;
        if (!focused)
            break;
        workspace = manager->get_active_workspace(server->focused_output());
        j["active_workspace"] = workspace->num;

        break;
    }
    case IPC_WORKSPACE_SET: {
        if (!server->config->ipc.bind_run) {
            notify_send("IPC", "%s",
                        "called workspace set but bind_run is disabled");
            break;
        }

        try {
            // try parsing an integer
            const uint32_t n = std::stoi(data);

            if (n < 1 || n > 10)
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
        Workspace *w, *t0;
        Toplevel *t, *t1;

        wl_list_for_each_safe(w, t0, &server->workspace_manager->workspaces,
                              link)
            wl_list_for_each_safe(t, t1, &w->toplevels, link)
            // toplevels are indexed by their pointer as title is
            // non-unique
            j[string_format("%p", t)] = {
                {"title", t->title()},
                {"class", t->app_id()},
                {"tag", t->tag},
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
        if (!server->config->ipc.bind_run) {
            notify_send("IPC", "%s", "called bind_run but it is disabled");
            break;
        }

        std::string bind_name;
        xkb_keysym_t keysym = 0;

        // parse bind name and keysym if applicable
        std::string::size_type s = data.find(",");
        if (s == std::string::npos)
            bind_name = data;
        else {
            bind_name = data.substr(0, s);
            try {
                int digit = std::stoi(data.substr(s + 1));
                if (digit < 1 || digit > 10)
                    throw std::invalid_argument("out of range");

                keysym = digit + XKB_KEY_0;
            } catch (std::invalid_argument &e) {
                notify_send("IPC", "invalid number `%s`: %s", data.c_str(),
                            e.what());
                break;
            }
        }

        // find bind name if present
        for (unsigned long i = 0; i <= BIND_WORKSPACE_WINDOW_TO; ++i)
            if (bind_name == BIND_NAMES[i]) {
                server->handle_bind(Bind{static_cast<BindName>(i), 0, keysym});
                break;
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
    case IPC_RULE_LIST: {
        std::vector<WindowRule *> rules = server->config->window_rules;
        for (unsigned long i = 0; i != rules.size(); ++i) {
            json res = {{"title", rules[i]->title},
                        {"class", rules[i]->class_},
                        {"tag", rules[i]->tag}};

            if (rules[i]->workspace)
                res["workspace"] = rules[i]->workspace;

            if (!rules[i]->output.empty())
                res["output"] = rules[i]->output;

            if (rules[i]->toplevel_state)
                switch (*rules[i]->toplevel_state) {
                case XDG_TOPLEVEL_STATE_FULLSCREEN:
                    res["toplevel_state"] = "fullscreen";
                    break;
                case XDG_TOPLEVEL_STATE_MAXIMIZED:
                    res["toplevel_state"] = "maximized";
                    break;
                default:
                    break;
                }

            if (rules[i]->pinned)
                res["pinned"] = rules[i]->pinned;

            if (rules[i]->geometry)
                res["geometry"] = {{"x", rules[i]->geometry->x},
                                   {"y", rules[i]->geometry->y},
                                   {"width", rules[i]->geometry->width},
                                   {"height", rules[i]->geometry->height}};

            j[i] = res;
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

    delete this;
}
