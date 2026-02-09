#include "WindowRule.h"
#include "Output.h"
#include "OutputManager.h"
#include "Server.h"
#include "Toplevel.h"
#include "Workspace.h"

WindowRule::WindowRule(std::string title_match, std::string class_match,
                       std::string tag_match, uint8_t matches_present)
    : title(title_match), class_(class_match), tag(tag_match),
      title_re(std::regex(title_match)), class_re(std::regex(class_match)),
      tag_re(std::regex(tag_match)), matches_present(matches_present) {
    geometry = new wlr_box;
}

WindowRule::~WindowRule() {
    if (toplevel_state)
        delete toplevel_state;
    delete geometry;
}

// see if toplevel matches window rule
bool WindowRule::matches(Toplevel *toplevel) {
    bool match = true;
    if (matches_present & WINDOW_RULE_TITLE)
        match &= std::regex_match(std::string(toplevel->get_title()), title_re);
    if (matches_present & WINDOW_RULE_CLASS)
        match &=
            std::regex_match(std::string(toplevel->get_app_id()), class_re);
    if (matches_present & WINDOW_RULE_TAG)
        match &= std::regex_match(toplevel->tag, tag_re);
    return match;
}

// apply each Rule in the WindowRule to a toplevel
void WindowRule::apply(Toplevel *toplevel) {
    Server *server = toplevel->server;

    // get target output
    Output *target_output = output.empty()
                                ? server->focused_output()
                                : server->output_manager->get_output(output);
    if (!target_output)
        target_output = server->focused_output();

    // get target workspace
    Workspace *target_workspace = workspace > 0
                                      ? target_output->get_workspace(workspace)
                                      : target_output->get_active();

    // determine if toplevel should be floating
    bool should_be_floating = false;
    switch (tiling_mode) {
    case TILING_MODE_FLOATING:
    case TILING_MODE_TILING:
        should_be_floating = true;
        break;
    case TILING_MODE_AUTO:
        should_be_floating = floating;
        if (!should_be_floating)
            should_be_floating = toplevel->should_be_floating();
    default:
        break;
    }

    // set toplevel floating state
    toplevel->is_floating = should_be_floating;

    // set toplevel workspace
    bool should_skip_auto_tile = should_be_floating;
    if (toplevel_state)
        should_skip_auto_tile =
            should_skip_auto_tile ||
            (*toplevel_state == XDG_TOPLEVEL_STATE_MAXIMIZED) ||
            (*toplevel_state == XDG_TOPLEVEL_STATE_FULLSCREEN);

    bool workspace_auto_tile = target_workspace->auto_tile;
    if (should_skip_auto_tile && workspace_auto_tile)
        target_workspace->auto_tile = false;

    target_workspace->add_toplevel(toplevel, false);

    if (should_skip_auto_tile && workspace_auto_tile)
        target_workspace->auto_tile = workspace_auto_tile;

    if (target_output->get_active() != target_workspace)
        toplevel->set_hidden(true);

    // set toplevel pinned state
    toplevel->pinned = pinned;

    // set toplevel geometry
    if (geometry)
        toplevel->set_position_size(*geometry);

    // set toplevel state
    if (toplevel_state)
        switch (*toplevel_state) {
        case XDG_TOPLEVEL_STATE_MAXIMIZED:
            toplevel->set_maximized(true);
            break;
        case XDG_TOPLEVEL_STATE_FULLSCREEN:
            toplevel->set_fullscreen(true);
            break;
        default:
            wlr_log(WLR_INFO, "Unhandled toplevel state %d", *toplevel_state);
            break;
        }
}
