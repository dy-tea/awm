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
        match &= std::regex_match(std::string(toplevel->title()), title_re);
    if (matches_present & WINDOW_RULE_CLASS)
        match &= std::regex_match(std::string(toplevel->app_id()), class_re);
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

    // set toplevel workspace
    target_workspace->add_toplevel(toplevel, false);
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
