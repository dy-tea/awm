#include "Server.h"
#include "wlr/util/log.h"
#include <nlohmann/json.hpp>
#include <wayland-util.h>
using json = nlohmann::json;

IPC::IPC(Server *server) : server(server) {
    // create file descriptor
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        wlr_log(WLR_ERROR, "%s", "failed to create IPC socket");
        return;
    }

    wlr_log(WLR_INFO, "starting IPC on socket %d", fd);

    // unlink old socket if present
    if (!unlink(path.c_str()))
        wlr_log(WLR_INFO, "removed old socket at path `%s`", path.c_str());

    // set socket address
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

    // bind socket
    if (bind(fd, reinterpret_cast<struct sockaddr *>(&addr),
             sizeof(struct sockaddr_un)) == -1) {
        wlr_log(WLR_ERROR, "failed to bind socket with fd `%d` on path `%s`",
                fd, path.c_str());
        return;
    }

    // listen for connections
    if (listen(fd, 5) == -1) {
        wlr_log(WLR_ERROR,
                "failed to listen on socket with fd `%d` on path `%s`", fd,
                path.c_str());
        return;
    }

    // start thread
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

            // run command
            std::string response = run(std::string(buffer, len));

            // write response to client
            if (write(client_fd, response.c_str(), strlen(response.c_str())) ==
                -1)
                wlr_log(WLR_ERROR,
                        "failed to write to client with fd `%d` on path `%s`",
                        client_fd, path.c_str());

            // close connection
            close(client_fd);
        }
    });
}

// run a received command
std::string IPC::run(std::string command) {
    std::string response;
    std::string token;
    std::stringstream ss(command);
    json j;

    wlr_log(WLR_INFO, "received command `%s`", command.c_str());

    if (std::getline(ss, token, ' ')) {
        if (token[0] == 'e') // exit
            server->exit();
        else if (token[0] == 'o') { // output
            if (std::getline(ss, token, ' ')) {
                if (token[0] == 'l') { // output list
                    Output *output, *tmp;
                    wl_list_for_each_safe(
                        output, tmp, &server->output_manager->outputs, link) {
                        j[output->wlr_output->name] = {
                            {"x", output->layout_geometry.x},
                            {"y", output->layout_geometry.y},
                            {"width", output->layout_geometry.width},
                            {"height", output->layout_geometry.height},
                            {"refresh", output->wlr_output->refresh / 1000.0},
                            {"scale", output->wlr_output->scale},
                            {"transform", output->wlr_output->transform},
                            {"adaptive",
                             output->wlr_output->adaptive_sync_supported},
                            {"enabled", output->wlr_output->enabled},
                            {"focused", output == server->focused_output()},
                            {"workspace", output->get_active()->num},
                        };

                        // below values may be null
                        if (output->wlr_output->description)
                            j[output->wlr_output->name]["description"] =
                                output->wlr_output->description;

                        if (output->wlr_output->make)
                            j[output->wlr_output->name]["make"] =
                                output->wlr_output->make;

                        if (output->wlr_output->model)
                            j[output->wlr_output->name]["model"] =
                                output->wlr_output->model;

                        if (output->wlr_output->serial)
                            j[output->wlr_output->name]["serial"] =
                                output->wlr_output->serial;
                    }

                    response = j.dump();
                } else if (token[0] == 'm') { // output modes
                    Output *output, *tmp;
                    wl_list_for_each_safe(
                        output, tmp, &server->output_manager->outputs, link) {
                        wlr_output_mode *mode, *tmp1;
                        int i = 0;
                        wl_list_for_each_safe(
                            mode, tmp1, &output->wlr_output->modes, link) {
                            j[output->wlr_output->name][i++] = {
                                {"refresh", mode->refresh / 1000.0},
                                {"width", mode->width},
                                {"height", mode->height},
                                {"preferred", mode->preferred},
                                {"selected",
                                 mode == output->wlr_output->current_mode},
                            };
                        }
                    }

                    response = j.dump();
                }
            }
        } else if (token[0] == 'w') { // workspace
            if (std::getline(ss, token, ' ')) {
                if (token[0] == 'l') { // workspace list
                    Output *output = server->focused_output();

                    j["max"] = output->max_workspace - 1;
                    int toplevel_count = 0;

                    Workspace *workspace, *tmp;
                    wl_list_for_each_safe(workspace, tmp, &output->workspaces,
                                          link) {
                        if (workspace == output->get_active())
                            j["active"] = workspace->num;

                        toplevel_count += wl_list_length(&workspace->toplevels);
                    }

                    j["toplevels"] = toplevel_count;

                    response = j.dump();
                } else if (token[0] == 's') { // workspace set
                    if (std::getline(ss, token, ' ')) {
                        try {
                            const uint32_t n = std::stoi(token);
                            if (n > server->focused_output()->max_workspace)
                                throw std::invalid_argument("out of range");

                            server->focused_output()->set_workspace(n);
                        } catch (std::invalid_argument &e) {
                            notify_send("invalid workspace number `%s`",
                                        token.c_str());
                            return "";
                        }
                    }
                }
            }
        } else if (token[0] == 't') { // toplevel
            if (std::getline(ss, token, ' ')) {
                if (token[0] == 'l') { // toplevel list
                    Output *o, *t0;
                    Workspace *w, *t1;
                    Toplevel *t, *t2;

                    wl_list_for_each_safe(
                        o, t0, &server->output_manager->outputs, link)
                        wl_list_for_each_safe(w, t1, &o->workspaces, link) {
                        wl_list_for_each_safe(t, t2, &w->toplevels, link) {
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
                        }
                    }

                    response = j.dump();
                }
            }
        } else if (token[0] == 'k') { // keyboard
            if (std::getline(ss, token, ' ')) {
                if (token[0] == 'l') { // keyboard list
                    j = {{"layout", server->config->keyboard_layout},
                         {"model", server->config->keyboard_model},
                         {"variant", server->config->keyboard_variant},
                         {"options", server->config->keyboard_options},
                         {"repeat_rate", server->config->repeat_rate},
                         {"repeat_delay", server->config->repeat_delay}};

                    response = j.dump();
                }
            }
        } else if (token[0] == 'd') { // device
            if (std::getline(ss, token, ' ')) {
                if (token[0] == 'l') { // device list
                    Keyboard *keyboard, *tmp;
                    int i = 0;
                    wl_list_for_each_safe(keyboard, tmp, &server->keyboards,
                                          link) {
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

                    response = j.dump();
                } else if (token[0] == 'c') { // device current
                    Keyboard *keyboard, *tmp;
                    wl_list_for_each_safe(keyboard, tmp, &server->keyboards,
                                          link) {
                        xkb_keymap *keymap = keyboard->wlr_keyboard->keymap;
                        xkb_state *state = keyboard->wlr_keyboard->xkb_state;
                        xkb_layout_index_t num_layouts =
                            keymap ? xkb_keymap_num_layouts(keymap) : 0;

                        for (xkb_layout_index_t layout_idx = 0;
                             layout_idx < num_layouts; layout_idx++) {

                            // this long name is nicer for clients
                            std::string layout_name =
                                xkb_keymap_layout_get_name(keymap, layout_idx);

                            // can be -1 if invalid
                            int layout_enabled =
                                xkb_state_layout_index_is_active(
                                    state, layout_idx,
                                    XKB_STATE_LAYOUT_EFFECTIVE);

                            j[keyboard->wlr_keyboard->base.name
                                  ? keyboard->wlr_keyboard->base.name
                                  : "default"][layout_idx] = {
                                {"enabled", layout_enabled},
                                {"layout", layout_name},
                            };
                        }
                    }

                    response = j.dump();
                }
            }
        } else
            notify_send("unknown command `%s`", token.c_str());
    }

    return response;
}

void IPC::stop() {
    // stop thread
    running = false;
    pthread_cancel(thread.native_handle());
    if (thread.joinable())
        thread.join();

    // close
    close(fd);

    // unlink
    if (unlink(path.c_str()))
        wlr_log(WLR_ERROR, "failed to unlink IPC socket at path `%s`",
                path.c_str());

    delete this;
}
