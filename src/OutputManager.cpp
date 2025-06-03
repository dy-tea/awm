#include "Server.h"
#include <map>

OutputManager::OutputManager(Server *server) : server(server) {
    layout = wlr_output_layout_create(server->display);
    wl_list_init(&outputs);

    // create managers
    wlr_xdg_output_manager =
        wlr_xdg_output_manager_v1_create(server->display, layout);
    wlr_output_manager = wlr_output_manager_v1_create(server->display);
    wlr_output_power_manager =
        wlr_output_power_manager_v1_create(server->display);

    // apply
    apply.notify = [](wl_listener *listener, void *data) {
        OutputManager *manager = wl_container_of(listener, manager, apply);

        manager->apply_config(static_cast<wlr_output_configuration_v1 *>(data),
                              false);
    };
    wl_signal_add(&wlr_output_manager->events.apply, &apply);

    // test
    test.notify = [](wl_listener *listener, void *data) {
        OutputManager *manager = wl_container_of(listener, manager, test);

        manager->apply_config(static_cast<wlr_output_configuration_v1 *>(data),
                              true);
    };
    wl_signal_add(&wlr_output_manager->events.test, &test);

    // destroy
    destroy.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        OutputManager *manager = wl_container_of(listener, manager, destroy);
        delete manager;
    };
    wl_signal_add(&wlr_output_manager->events.destroy, &destroy);

    // new_output
    new_output.notify = [](wl_listener *listener, void *data) {
        // new display / monitor available
        OutputManager *manager = wl_container_of(listener, manager, new_output);
        auto *wlr_output = static_cast<struct wlr_output *>(data);

        // create new output
        new Output(manager->server, wlr_output);
    };
    wl_signal_add(&server->backend->events.new_output, &new_output);

    // change
    change.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        wlr_output_configuration_v1 *config =
            wlr_output_configuration_v1_create();
        OutputManager *manager = wl_container_of(listener, manager, change);

        Output *output;
        wl_list_for_each(output, &manager->outputs, link) {
            // get the config head for each output
            wlr_output_configuration_head_v1 *config_head =
                wlr_output_configuration_head_v1_create(config,
                                                        output->wlr_output);

            // get the output box
            wlr_box output_box{};
            wlr_output_layout_get_box(manager->layout, output->wlr_output,
                                      &output_box);

            // mark the output enabled if it's switched off but not disabled
            config_head->state.enabled = !wlr_box_empty(&output_box);
            config_head->state.x = output_box.x;
            config_head->state.y = output_box.y;
        }

        // update the configuration
        wlr_output_manager_v1_set_configuration(manager->wlr_output_manager,
                                                config);
    };
    wl_signal_add(&layout->events.change, &change);

    // set power mode
    set_mode.notify = [](wl_listener *listener, void *data) {
        OutputManager *manager = wl_container_of(listener, manager, set_mode);
        auto event = static_cast<wlr_output_power_v1_set_mode_event *>(data);
        wlr_output_state state{};
        Output *output = manager->server->get_output(event->output);

        if (!output)
            return;

        wlr_output_state_set_enabled(&state, event->mode);
        wlr_output_commit_state(event->output, &state);
        wlr_output_state_finish(&state);

        manager->arrange();
    };
    wl_signal_add(&wlr_output_power_manager->events.set_mode, &set_mode);
}

OutputManager::~OutputManager() {
    wl_list_remove(&apply.link);
    wl_list_remove(&test.link);
    wl_list_remove(&destroy.link);
    wl_list_remove(&new_output.link);
    wl_list_remove(&change.link);
    wl_list_remove(&set_mode.link);
}

void OutputManager::apply_config(wlr_output_configuration_v1 *cfg,
                                 bool test_only) const {
    // create a map of output names to configs
    std::map<std::string, OutputConfig *> config_map;

    // add existing configs
    for (OutputConfig *c : server->config->outputs)
        config_map[c->name] = c;

    // override with new configs from cfg
    wlr_output_configuration_head_v1 *config_head;
    wl_list_for_each(config_head, &cfg->heads, link) {
        std::string name = config_head->state.output->name;
        config_map[name] = new OutputConfig(config_head);
    }

    // apply each config
    bool success = true;
    Output *output, *tmp;
    wl_list_for_each_safe(output, tmp, &outputs, link) {
        OutputConfig *oc = config_map[output->wlr_output->name];
        success &= output->apply_config(oc, test_only);
    }

    // send cfg status
    if (success)
        wlr_output_configuration_v1_send_succeeded(cfg);
    else
        wlr_output_configuration_v1_send_failed(cfg);

    // notify ipc
    if (server->ipc)
        server->ipc->notify_clients(IPC_OUTPUT_MODES);

    arrange();
}

// get output by wlr_output
Output *OutputManager::get_output(const wlr_output *wlr_output) {
    // check each output
    Output *output, *tmp;
    wl_list_for_each_safe(output, tmp, &outputs,
                          link) if (output->wlr_output ==
                                    wlr_output) return output;

    // no output found
    wlr_log(WLR_ERROR, "could not find output of pointer %p",
            (void *)wlr_output);
    return nullptr;
}

// get the output based on screen coordinates
Output *OutputManager::output_at(const double x, const double y) {
    const wlr_output *wlr_output = wlr_output_layout_output_at(layout, x, y);

    // no output found
    if (!wlr_output)
        return nullptr;

    // get associated output
    return get_output(wlr_output);
}

// arrange layer shell layers on each output
void OutputManager::arrange() const {
    // arrange each output
    Output *output, *tmp;
    wl_list_for_each_safe(output, tmp, &outputs, link) {
        // tell outputs to update their positions
        output->update_position();

        // arrange layers in z order
        wlr_scene_node_set_position(&output->layers.background->node,
                                    output->layout_geometry.x,
                                    output->layout_geometry.y);
        wlr_scene_node_set_position(&output->layers.bottom->node,
                                    output->layout_geometry.x,
                                    output->layout_geometry.y);
        wlr_scene_node_set_position(&output->layers.top->node,
                                    output->layout_geometry.x,
                                    output->layout_geometry.y);
        wlr_scene_node_set_position(&output->layers.overlay->node,
                                    output->layout_geometry.x,
                                    output->layout_geometry.y);
    }

    // update lock background
    const wlr_box full = full_geometry();
    wlr_scene_node_set_position(&server->lock_background->node, full.x, full.y);
    wlr_scene_rect_set_size(server->lock_background, full.width, full.height);
}

// get the full geometry of all outputs
wlr_box OutputManager::full_geometry() const {
    wlr_box box{};
    wlr_output_layout_get_box(layout, nullptr, &box);
    return box;
}
