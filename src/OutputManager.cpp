#include "Server.h"
#include <map>

OutputManager::OutputManager(struct Server *server) {
    this->server = server;

    layout = wlr_output_layout_create(server->wl_display);
    wl_list_init(&outputs);

    this->wlr_xdg_output_manager =
        wlr_xdg_output_manager_v1_create(server->wl_display, layout);
    this->wlr_output_manager = wlr_output_manager_v1_create(server->wl_display);

    // apply
    apply.notify = [](struct wl_listener *listener, void *data) {
        OutputManager *manager = wl_container_of(listener, manager, apply);

        manager->apply_config(static_cast<wlr_output_configuration_v1 *>(data),
                              false);
    };
    wl_signal_add(&wlr_output_manager->events.apply, &apply);

    // test
    test.notify = [](struct wl_listener *listener, void *data) {
        OutputManager *manager = wl_container_of(listener, manager, test);

        manager->apply_config(static_cast<wlr_output_configuration_v1 *>(data),
                              true);
    };
    wl_signal_add(&wlr_output_manager->events.test, &test);

    // destroy
    destroy.notify = [](struct wl_listener *listener, void *data) {
        OutputManager *manager = wl_container_of(listener, manager, destroy);
        delete manager;
    };
    wl_signal_add(&wlr_output_manager->events.destroy, &destroy);

    // new_output
    new_output.notify = [](struct wl_listener *listener, void *data) {
        // new display / monitor available
        struct OutputManager *manager =
            wl_container_of(listener, manager, new_output);
        struct Server *server = manager->server;
        struct wlr_output *wlr_output = static_cast<struct wlr_output *>(data);

        // set allocator and renderer for output
        wlr_output_init_render(wlr_output, server->allocator, server->renderer);

        // add to outputs list
        struct Output *output = new Output(server, wlr_output);
        wl_list_insert(&manager->outputs, &output->link);

        // find matching config
        OutputConfig *matching = nullptr;
        for (OutputConfig *config : server->config->outputs) {
            if (config->name == wlr_output->name) {
                matching = config;
                break;
            }
        }

        // apply the config
        bool config_success =
            matching && manager->apply_to_output(output, matching, false);

        // fallback
        if (!config_success) {
            wlr_log(WLR_INFO, "using fallback mode for output %s",
                    output->wlr_output->name);

            // create new state
            struct wlr_output_state state;
            wlr_output_state_init(&state);

            // use preferred mode
            wlr_output_state_set_enabled(&state, true);
            wlr_output_state_set_mode(&state,
                                      wlr_output_preferred_mode(wlr_output));

            // commit state
            config_success = wlr_output_commit_state(wlr_output, &state);
            wlr_output_state_finish(&state);
        }

        // position
        wlr_output_layout_output *output_layout_output =
            matching && config_success
                ? wlr_output_layout_add(manager->layout, wlr_output,
                                        matching->x, matching->y)
                : wlr_output_layout_add_auto(manager->layout, wlr_output);

        // add to scene output
        wlr_scene_output *scene_output =
            wlr_scene_output_create(server->scene, wlr_output);
        wlr_scene_output_layout_add_output(server->scene_layout,
                                           output_layout_output, scene_output);

        // set usable area
        wlr_output_layout_get_box(manager->layout, wlr_output,
                                  &output->usable_area);
    };
    wl_signal_add(&server->backend->events.new_output, &new_output);

    // change
    change.notify = [](struct wl_listener *listener, void *data) {
        struct wlr_output_configuration_v1 *config =
            wlr_output_configuration_v1_create();
        struct OutputManager *manager =
            wl_container_of(listener, manager, change);

        struct Output *output;
        wl_list_for_each(output, &manager->outputs, link) {
            // get the config head for each output
            struct wlr_output_configuration_head_v1 *config_head =
                wlr_output_configuration_head_v1_create(config,
                                                        output->wlr_output);

            // get the output box
            struct wlr_box output_box;
            wlr_output_layout_get_box(manager->layout, output->wlr_output,
                                      &output_box);

            // mark the output enabled if it's swithed off but not disabled
            config_head->state.enabled = !wlr_box_empty(&output_box);
            config_head->state.x = output_box.x;
            config_head->state.y = output_box.y;
        }

        // update the configuration
        wlr_output_manager_v1_set_configuration(manager->wlr_output_manager,
                                                config);
    };
    wl_signal_add(&layout->events.change, &change);
}

OutputManager::~OutputManager() {
    wl_list_remove(&apply.link);
    wl_list_remove(&test.link);
    wl_list_remove(&destroy.link);
    wl_list_remove(&new_output.link);
    wl_list_remove(&change.link);
}

bool OutputManager::apply_to_output(struct Output *output,
                                    struct OutputConfig *config,
                                    bool test_only) {
    // create new state
    struct wlr_output *wlr_output = output->wlr_output;

    // create output state
    struct wlr_output_state state;
    wlr_output_state_init(&state);

    // enabled
    wlr_output_state_set_enabled(&state, config->enabled);

    if (config->enabled) {
        // set mode
        bool mode_set = false;
        if (config->width > 0 && config->height > 0 && config->refresh > 0) {
            // find matching mode
            struct wlr_output_mode *mode, *best_mode = nullptr;
            wl_list_for_each(
                mode, &wlr_output->modes,
                link) if ((mode->width == config->width &&
                           mode->height == config->height) &&
                          (!best_mode ||
                           (abs((int)(mode->refresh / 1000.0 -
                                      config->refresh)) < 1.5 &&
                            abs((int)(mode->refresh / 1000.0 -
                                      config->refresh)) <
                                abs((int)(best_mode->refresh / 1000.0 -
                                          config->refresh))))) best_mode = mode;

            if (best_mode) {
                wlr_output_state_set_mode(&state, best_mode);
                mode_set = true;
            }
        }

        // set to preferred mode if not set
        if (!mode_set) {
            wlr_output_state_set_mode(&state,
                                      wlr_output_preferred_mode(wlr_output));
            wlr_log(WLR_INFO, "using fallback mode for output %s",
                    config->name.c_str());
        }

        // scale
        if (config->scale > 0)
            wlr_output_state_set_scale(&state, config->scale);

        // transform
        wlr_output_state_set_transform(&state, config->transform);

        // adaptive sync
        wlr_output_state_set_adaptive_sync_enabled(&state,
                                                   config->adaptive_sync);
    }

    bool success;
    if (test_only)
        success = wlr_output_test_state(wlr_output, &state);
    else {
        success = wlr_output_commit_state(wlr_output, &state);
        if (success) {
            // update position
            wlr_output_layout_add(layout, wlr_output, config->x, config->y);
            // rearrange
            output->arrange_layers();
        }
    }

    wlr_output_state_finish(&state);
    return success;
}

void OutputManager::apply_config(struct wlr_output_configuration_v1 *cfg,
                                 bool test_only) {
    // create a map of output names to configs
    std::map<std::string, OutputConfig *> config_map;

    // add existing configs
    for (OutputConfig *c : server->config->outputs)
        config_map[c->name] = c;

    // oveerride with new configs from cfg
    struct wlr_output_configuration_head_v1 *config_head;
    wl_list_for_each(config_head, &cfg->heads, link) {
        std::string name = config_head->state.output->name;
        config_map[name] = new OutputConfig(config_head);
    }

    // apply each config
    bool success = true;
    Output *output, *tmp;
    wl_list_for_each_safe(output, tmp, &outputs, link) {
        OutputConfig *oc = config_map[output->wlr_output->name];
        success &= apply_to_output(output, oc, test_only);
    }

    // send cfg status
    if (success)
        wlr_output_configuration_v1_send_succeeded(cfg);
    else
        wlr_output_configuration_v1_send_failed(cfg);
}

// get output by wlr_output
struct Output *OutputManager::get_output(struct wlr_output *wlr_output) {
    // check each output
    Output *output, *tmp;
    wl_list_for_each_safe(output, tmp, &outputs,
                          link) if (output->wlr_output ==
                                    wlr_output) return output;

    // no output found
    wlr_log(WLR_ERROR, "could not find output");
    return nullptr;
}

// get the output based on screen coordinates
struct Output *OutputManager::output_at(double x, double y) {
    struct wlr_output *wlr_output = wlr_output_layout_output_at(layout, x, y);

    // no output found
    if (wlr_output == NULL)
        return NULL;

    // get associated output
    return get_output(wlr_output);
}

// arrange layer shell layers on each output
void OutputManager::arrange() {
    struct Output *output, *tmp;
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
}
