#include "WorkspaceManager.h"
#include "IPC.h"
#include "Server.h"

WorkspaceManager::WorkspaceManager(Server *server) : server(server) {
    wl_list_init(&workspaces);
}

WorkspaceManager::~WorkspaceManager() {
    Workspace *workspace, *tmp;
    wl_list_for_each_safe(workspace, tmp, &workspaces, link) delete workspace;

    // delete orphaned workspaces
    for (auto [a, b] : orphaned_outputs_map)
        for (Workspace *workspace : b.workspaces)
            delete workspace;
}

// Create a new workspace for an output
Workspace *WorkspaceManager::new_workspace(Output *output, uint32_t num) {
    // create new workspace
    Workspace *workspace = new Workspace(output, num);
    wl_list_insert(&workspaces, &workspace->link);

    // notify clients
    if (server->ipc)
        server->ipc->notify_clients({IPC_OUTPUT_LIST, IPC_WORKSPACE_LIST});

    return workspace;
}

// get workspace by number, optionally filtering by output
Workspace *WorkspaceManager::get_workspace(uint32_t num, Output *output) const {
    Workspace *workspace, *tmp;
    wl_list_for_each_safe(
        workspace, tmp, &workspaces,
        link) if (workspace->num == num &&
                  (!output || workspace->output == output)) return workspace;
    return nullptr;
}

// get the active workspace for an output
Workspace *WorkspaceManager::get_active_workspace(Output *output) const {
    if (!output)
        return nullptr;

    // find first workspace of output
    Workspace *workspace, *tmp;
    wl_list_for_each_safe(workspace, tmp, &workspaces,
                          link) if (workspace->output ==
                                    output) return workspace;
    return nullptr;
}

// set the active workspace
bool WorkspaceManager::set_workspace(Workspace *workspace) {
    if (!workspace)
        return false;

    Output *output = workspace->output;
    Workspace *previous = get_active_workspace(output);

    // invalid workspace
    if (!previous)
        return false;

    // workspace is already active
    if (workspace == previous)
        return true;

    // hide workspace we are moving from
    previous->set_hidden(true);

    // get pinned toplevels from previous workspace
    std::vector<Toplevel *> pinned = previous->pinned();

    // set workspace to active
    wl_list_remove(&workspace->link);
    wl_list_insert(&workspaces, &workspace->link);

    // move pinned toplevels to new workspace
    for (Toplevel *toplevel : pinned)
        previous->move_to(toplevel, workspace);

    // focus new workspace
    workspace->focus();

    return true;
}

// set active workspace by number for an output
bool WorkspaceManager::set_workspace(uint32_t num, Output *output) {
    return set_workspace(get_workspace(num, output));
}

// find workspace containing given toplevel
Workspace *
WorkspaceManager::get_workspace_for_toplevel(Toplevel *toplevel) const {
    if (!toplevel)
        return nullptr;

    Workspace *workspace, *tmp;
    wl_list_for_each_safe(
        workspace, tmp, &workspaces,
        link) if (workspace->contains(toplevel)) return workspace;
    return nullptr;
}

// turn all workspaces into orphans
void WorkspaceManager::orphanize_workspaces(Output *output) {
    uint32_t active_num = get_active_workspace(output)->num;
    std::vector<Workspace *> orphans;

    Workspace *ws, *tmp;
    wl_list_for_each_safe(ws, tmp, &workspaces,
                          link) if (ws->output == output) {
        ws->output = nullptr;
        orphans.push_back(ws);
        wl_list_remove(&ws->link);
    }

    orphaned_outputs_map[output->wlr_output->name] =
        OrphanedOutput{active_num, orphans};
}

// adopt orphaned workspaces if output name matches an orphaned output
bool WorkspaceManager::adopt_workspaces(Output *output) {
    if (!output)
        return false;

    auto it = orphaned_outputs_map.find(output->wlr_output->name);
    if (it == orphaned_outputs_map.end())
        return false;

    const OrphanedOutput &orphaned = it->second;

    for (Workspace *workspace : orphaned.workspaces) {
        workspace->output = output;
        wl_list_insert(&workspaces, &workspace->link);
    }

    set_workspace(orphaned.active_num, output);
    orphaned_outputs_map.erase(it);
    return true;
}
