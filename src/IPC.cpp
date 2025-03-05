#include "Server.h"
#include <nlohmann/json.hpp>
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
                            {"refresh", output->wlr_output->refresh},
                            {"scale", output->wlr_output->scale},
                            {"transform", output->wlr_output->transform},
                            {"adaptive",
                             output->wlr_output->adaptive_sync_supported},
                            {"enabled", output->wlr_output->enabled},
                            {"focused", output == server->focused_output()},
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
                }
            }
        } else if (token[0] == 'w') { // workspace
            if (std::getline(ss, token, ' ')) {
                if (token[0] == 'l') { // workspace list
                    Output *output = server->focused_output();
                    json j;

                    Workspace *workspace, *tmp;
                    wl_list_for_each_safe(workspace, tmp, &output->workspaces,
                                          link) {
                        Toplevel *toplevel, *tmp1;

                        j[workspace->num] = {
                            {"num", workspace->num},
                            {"focused", workspace == output->get_active()},
                        };

                        wl_list_for_each_safe(toplevel, tmp1,
                                              &workspace->toplevels, link) {
                            j[workspace->num]["toplevels"]
                             [string_format("%p", toplevel)] = {
                                 {"title", toplevel->title()},
                                 {"x", toplevel->geometry.x},
                                 {"y", toplevel->geometry.y},
                                 {"width", toplevel->geometry.width},
                                 {"height", toplevel->geometry.height},
                                 {"focused",
                                  toplevel == workspace->active_toplevel},
                             };
                        }
                    }

                    response = j.dump();
                }
            }
        } else if (token[0] == 't') { // toplevel
            if (std::getline(ss, token, ' ')) {
                if (token[0] == 'l') { // toplevel list
                    Output *o, *t0;
                    Workspace *w, *t1;
                    Toplevel *t, *t2;
                    json j;

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
#ifdef XWAYLAND
                                {"xwayland", !t->xdg_toplevel},
#endif
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
