#include "WindowRule.h"
#include "Output.h"
#include "OutputManager.h"
#include "Server.h"
#include "Toplevel.h"
#include "Workspace.h"

WindowRule::WindowRule(std::string title_match, std::string class_match,
                       std::string tag_match, uint8_t matches_present)
    : title_re(std::regex(title_match)), class_re(std::regex(class_match)),
      tag_re(std::regex(tag_match)), matches_present(matches_present) {
    geometry = new wlr_box;
}

WindowRule::~WindowRule() {
    if (toplevel_state)
        delete toplevel_state;
    delete geometry;
}

void WindowRule::add_rule(Rules rule_name, int data) {
    rule_count++;

    switch (rule_name) {
    case RULES_WORKSPACE: {
        workspace = data;
        break;
    }
    case RULES_TOPLEVEL_X: {
        geometry->x = data;
        break;
    }
    case RULES_TOPLEVEL_Y: {
        geometry->y = data;
        break;
    }
    case RULES_TOPLEVEL_W: {
        geometry->width = data;
        break;
    }
    case RULES_TOPLEVEL_H: {
        geometry->height = data;
        break;
    }
    default:
        throw std::runtime_error("Invalid rule type for int");
    }
}

void WindowRule::add_rule(Rules rule_name, const std::string &data) {
    if (rule_name == RULES_OUTPUT) {
        output = data;
        rule_count++;
    } else {
        throw std::runtime_error("Invalid rule type for std::string");
    }
}

void WindowRule::add_rule(Rules rule_name, xdg_toplevel_state *data) {
    if (rule_name == RULES_TOPLEVEL_STATE) {
        toplevel_state = data;
        rule_count++;
    } else {
        throw std::runtime_error("Invalid rule type for xdg_toplevel_state*");
    }
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
