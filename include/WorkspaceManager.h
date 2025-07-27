#pragma once

#include "Output.h"
#include "Workspace.h"
#include <map>
#include <string>

struct OrphanedOutput {
    uint32_t active_num;
    std::vector<Workspace *> workspaces;
};

struct WorkspaceManager {
    Server *server;
    struct wl_list workspaces;
    std::map<std::string, OrphanedOutput> orphaned_outputs_map;

    WorkspaceManager(Server *server);
    ~WorkspaceManager();

    Workspace *new_workspace(Output *output, uint32_t num = 0);
    Workspace *get_workspace(uint32_t num, Output *output = nullptr) const;
    Workspace *get_active_workspace(Output *output) const;
    bool set_workspace(Workspace *workspace);
    bool set_workspace(uint32_t num, Output *output);

    Workspace *get_workspace_for_toplevel(Toplevel *toplevel) const;

    void orphanize_workspaces(Output *output);
    bool adopt_workspaces(Output *output);
};
