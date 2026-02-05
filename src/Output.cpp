#include "Output.h"
#include "Config.h"
#include "LayerSurface.h"
#include "OutputManager.h"
#include "Server.h"
#include "Toplevel.h"
#include "WorkspaceManager.h"
#include "wlr.h"
#include <drm_fourcc.h>

RenderBitDepth bit_depth_from_format(uint32_t format) {
    switch (format) {
    case DRM_FORMAT_XRGB2101010:
    case DRM_FORMAT_XBGR2101010:
        return RENDER_BIT_DEPTH_10;
    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_ARGB8888:
        return RENDER_BIT_DEPTH_8;
    case DRM_FORMAT_RGB565:
        return RENDER_BIT_DEPTH_6;
    default:
        return RENDER_BIT_DEPTH_DEFAULT;
    }
}

static int output_repaint_timer(void *data) {
    Output *output = static_cast<Output *>(data);

    output->wlr_output->frame_pending = false;
    if (!output->wlr_output->enabled)
        return 0;

    // // FIXME: uncomment when we have ICC profile support
    // color_transform_wlr_scene_output_state_options opts = {nullptr,
    // output->color_transform,
    //                                        nullptr};

    wlr_scene_output *scene_output = output->scene_output;
    if (!wlr_scene_output_needs_frame(scene_output))
        return 0;

    wlr_output_state pending;
    wlr_output_state_init(&pending);
    // wlr_scene_output_state_options opts = {};
    // if (!wlr_scene_output_build_state(output->scene_output, &pending, &opts))
    // {
    //     wlr_output_state_finish(&pending);
    //     return 0;
    // }

    // enable tearing for fullscreen toplevels with tearing hint
    Workspace *workspace = output->get_active();
    if (workspace && output->allow_tearing) {
        std::vector<Toplevel *> fullscreen_toplevels =
            workspace->fullscreen_toplevels();
        if (!fullscreen_toplevels.empty()) {
            for (Toplevel *t : fullscreen_toplevels) {
                if (t->tearing_hint ==
                    WP_TEARING_CONTROL_V1_PRESENTATION_HINT_ASYNC) {
                    pending.tearing_page_flip = true;
                    if (!wlr_output_test_state(output->wlr_output, &pending))
                        pending.tearing_page_flip = false;
                }
            }
        }
    }

    if (!wlr_output_commit_state(output->wlr_output, &pending))
        wlr_log(WLR_ERROR, "failed to page-flip on output %s",
                output->wlr_output->name);

    wlr_output_state_finish(&pending);
    return 0;
}

Output::Output(Server *server, struct wlr_output *wlr_output)
    : server(server), wlr_output(wlr_output) {
    // add to outputs list
    OutputManager *manager = server->output_manager;
    wl_list_insert(&manager->outputs, &link);

    // name headless outputs
    if (wlr_output_is_headless(wlr_output)) {
        headless = true;
        std::string name =
            "HEADLESS-" + std::to_string(manager->max_headless++);
        wlr_output_set_name(wlr_output, name.c_str());
    }

    wlr_log(WLR_INFO, "new output %s", wlr_output->name);

    // do not configure non-desktop outputs
    if (wlr_output->non_desktop) {
        if (server->wlr_drm_lease_manager)
            wlr_drm_lease_v1_manager_offer_output(server->wlr_drm_lease_manager,
                                                  wlr_output);
        return;
    }

    // set allocator and renderer for output
    if (!wlr_output_init_render(wlr_output, server->allocator,
                                server->renderer)) {
        wlr_log(WLR_ERROR, "failed to init render on output %s",
                wlr_output->name);
        delete this;
        return;
    }

    // create initial workspaces through WorkspaceManager
    if (!server->workspace_manager->adopt_workspaces(this)) {
        for (int i = 1; i <= 10; ++i)
            server->workspace_manager->new_workspace(this, i);
        set_workspace(1);
    }

    // create layers
    layers.background = wlr_scene_tree_create(server->layers.background);
    layers.bottom = wlr_scene_tree_create(server->layers.bottom);
    layers.top = wlr_scene_tree_create(server->layers.top);
    layers.overlay = wlr_scene_tree_create(server->layers.overlay);

    // send arrange
    manager->arrange();

    // point output data to this
    wlr_output->data = this;

    // frame
    frame.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        // called once per frame
        Output *output = wl_container_of(listener, output, frame);
        if (!output->enabled || !output->wlr_output->enabled)
            return;

        // calculate delay
        int msec_re = 0;
        if (output->max_render_time) {
            timespec now{};
            clock_gettime(CLOCK_MONOTONIC, &now);

            const long NSEC_IN_SEC = 1000000000;
            timespec predicted_re = output->last_present;
            predicted_re.tv_nsec += output->refresh_nsec % NSEC_IN_SEC;
            predicted_re.tv_sec += output->refresh_nsec / NSEC_IN_SEC;
            if (predicted_re.tv_nsec >= NSEC_IN_SEC) {
                predicted_re.tv_nsec -= NSEC_IN_SEC;
                predicted_re.tv_sec++;
            }

            if (predicted_re.tv_nsec >= now.tv_nsec) {
                long nsec_re =
                    (predicted_re.tv_sec - now.tv_sec) * NSEC_IN_SEC +
                    (predicted_re.tv_nsec - now.tv_nsec);
                msec_re = nsec_re / 1000000;
            }
        }

        // send repaint
        int delay = msec_re - output->max_render_time;
        if (delay < 1)
            wl_event_source_timer_update(output->repaint_timer, 0);
        else {
            output->wlr_output->frame_pending = true;
            wl_event_source_timer_update(output->repaint_timer, delay);
        }

        // render scene
        wlr_scene_output *scene_output = wlr_scene_get_scene_output(
            output->server->scene, output->wlr_output);
        wlr_scene_output_commit(scene_output, nullptr);

        // get frame time
        timespec now{};
        clock_gettime(CLOCK_MONOTONIC, &now);
        wlr_scene_output_send_frame_done(scene_output, &now);
    };
    wl_signal_add(&wlr_output->events.frame, &frame);

    // present
    present.notify = [](wl_listener *listener, void *data) {
        Output *output = wl_container_of(listener, output, present);
        auto *event = static_cast<wlr_output_event_present *>(data);
        if (!output->enabled || !event->presented)
            return;

        output->last_present = event->when;
        output->refresh_nsec = event->refresh;
    };
    wl_signal_add(&wlr_output->events.present, &present);

    // request_state
    request_state.notify = [](wl_listener *listener, void *data) {
        Output *output = wl_container_of(listener, output, request_state);

        const auto *event = static_cast<wlr_output_event_request_state *>(data);

        wlr_output_commit_state(output->wlr_output, event->state);
        output->arrange_layers();
        output->update_position();
    };
    wl_signal_add(&wlr_output->events.request_state, &request_state);

    // destroy
    destroy.notify = [](wl_listener *listener, [[maybe_unused]] void *data) {
        Output *output = wl_container_of(listener, output, destroy);
        delete output;
    };
    wl_signal_add(&wlr_output->events.destroy, &destroy);

    // find matching config
    OutputConfig *matching = nullptr;
    for (OutputConfig *config : server->config->outputs) {
        if (config->name == wlr_output->name) {
            matching = config;
            break;
        }
    }

    // apply the config
    bool config_success = matching && apply_config(matching, false);

    // fallback config
    if (!config_success) {
        wlr_log(WLR_INFO, "using fallback mode for output %s",
                wlr_output->name);

        // create new state
        wlr_output_state state{};
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
            ? wlr_output_layout_add(manager->layout, wlr_output, matching->x,
                                    matching->y)
            : wlr_output_layout_add_auto(manager->layout, wlr_output);

    // add to scene output
    scene_output = wlr_scene_output_create(server->scene, wlr_output);
    wlr_scene_output_layout_add_output(server->scene_layout,
                                       output_layout_output, scene_output);

    // set geometry
    wlr_output_layout_get_box(manager->layout, wlr_output, &layout_geometry);
    memcpy(&usable_area, &layout_geometry, sizeof(wlr_box));

    // initialize repaint timer
    repaint_timer =
        wl_event_loop_add_timer(server->event_loop, output_repaint_timer, this);
}

Output::~Output() {
    wl_list_remove(&link);
    if (wlr_output->non_desktop)
        return;

    if (server->workspace_manager)
        server->workspace_manager->orphanize_workspaces(this);

    wl_list_remove(&frame.link);
    wl_list_remove(&present.link);
    wl_list_remove(&request_state.link);
    wl_list_remove(&destroy.link);

    wl_event_source_remove(repaint_timer);
    repaint_timer = nullptr;
}

// arrange all layers
void Output::arrange_layers() {
    // output must be enabled to have an effective resolution
    if (!enabled)
        return;

    wlr_box usable = {};
    wlr_output_effective_resolution(wlr_output, &usable.width, &usable.height);
    const wlr_box full_area = usable;

    // exclusive surfaces
    arrange_layer_surface(&full_area, &usable, layers.overlay, true);
    arrange_layer_surface(&full_area, &usable, layers.top, true);
    arrange_layer_surface(&full_area, &usable, layers.bottom, true);
    arrange_layer_surface(&full_area, &usable, layers.background, true);

    // non-exclusive surfaces
    arrange_layer_surface(&full_area, &usable, layers.overlay, false);
    arrange_layer_surface(&full_area, &usable, layers.top, false);
    arrange_layer_surface(&full_area, &usable, layers.bottom, false);
    arrange_layer_surface(&full_area, &usable, layers.background, false);

    // check if usable area changed
    if (memcmp(&usable, &usable_area, sizeof(wlr_box)) != 0)
        usable_area = usable;

    // handle keyboard interactive layers
    LayerSurface *topmost = nullptr;
    wlr_scene_tree *layers_above_shell[] = {layers.overlay, layers.top};

    for (const wlr_scene_tree *layer : layers_above_shell) {
        wlr_scene_node *node;
        wl_list_for_each_reverse(node, &layer->children, link) {
            LayerSurface *surface = static_cast<LayerSurface *>(node->data);
            if (server->locked || !surface ||
                surface->wlr_layer_surface->current.keyboard_interactive !=
                    ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE ||
                !surface->wlr_layer_surface->surface->mapped)
                continue;

            topmost = surface;
            break;
        }
        if (topmost)
            break;
    }
}

// arrange a surface layer
void Output::arrange_layer_surface(const wlr_box *full, wlr_box *usable,
                                   const wlr_scene_tree *tree,
                                   const bool exclusive) {
    wlr_scene_node *node;
    wl_list_for_each(node, &tree->children, link) {
        const LayerSurface *surface = static_cast<LayerSurface *>(node->data);
        if (!surface || !surface->wlr_layer_surface->initialized)
            continue;

        if ((surface->wlr_layer_surface->current.exclusive_zone > 0) !=
            exclusive)
            continue;

        wlr_scene_layer_surface_v1_configure(surface->scene_layer_surface, full,
                                             usable);
    }
}

// get a layer shell layer
wlr_scene_tree *
Output::shell_layer(const enum zwlr_layer_shell_v1_layer layer) const {
    switch (layer) {
    case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
        return layers.background;
    case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
        return layers.bottom;
    case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
        return layers.top;
    case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
        return layers.overlay;
    default:
        throw std::runtime_error("unreachable: invalid layer");
        return nullptr;
    }
}

bool Output::supports_hdr() {
    std::string reason = "";
    if (!(wlr_output->supported_primaries & WLR_COLOR_NAMED_PRIMARIES_BT2020))
        reason = "BT2020 primaries not supported";
    else if (!(wlr_output->supported_transfer_functions &
               WLR_COLOR_TRANSFER_FUNCTION_ST2084_PQ))
        reason = "ST2084 PQ transfer function not supported";
    else if (!server->renderer->features.output_color_transform)
        reason = "renderer does not support output color transform";

    bool supported = reason.empty();
    if (!supported)
        wlr_log(WLR_ERROR, "output %s does not support HDR: %s",
                wlr_output->name, reason.c_str());

    return supported;
}

// create a new workspace for this output
Workspace *Output::new_workspace() {
    return server->workspace_manager->new_workspace(this);
}

Workspace *Output::get_active() {
    return server->workspace_manager->get_active_workspace(this);
}

Workspace *Output::get_workspace(const uint32_t n) {
    return server->workspace_manager->get_workspace(n, this);
}

// change the focused workspace to passed workspace
bool Output::set_workspace(Workspace *workspace) {
    return server->workspace_manager->set_workspace(workspace);
}

// change the focused workspace to workspace n
bool Output::set_workspace(const uint32_t n) {
    return server->workspace_manager->set_workspace(n, this);
}

// update layout geometry
void Output::update_position() {
    wlr_output_layout_get_box(server->output_manager->layout, wlr_output,
                              &layout_geometry);
}

// apply a config to the output
bool Output::apply_config(OutputConfig *config, const bool test_only) {
    // create output state
    wlr_output_state state{};
    wlr_output_state_init(&state);

    // enabled
    wlr_output_state_set_enabled(&state, config->enabled);

    if (config->enabled) {
        // set mode
        bool mode_set = false;
        if (config->width > 0 && config->height > 0 && config->refresh > 0) {
            // find matching mode
            wlr_output_mode *mode, *best_mode = nullptr;
            wl_list_for_each(mode, &wlr_output->modes, link)
                // find mode where width and height are equal to OutputConfig
                // and refresh rate is within 1.5
                if ((mode->width == config->width &&
                     mode->height == config->height) &&
                    (!best_mode ||
                     (abs(static_cast<int>(mode->refresh / 1000.0 -
                                           config->refresh)) < 1.5 &&
                      abs(static_cast<int>(mode->refresh / 1000.0 -
                                           config->refresh)) <
                          abs(static_cast<int>(best_mode->refresh / 1000.0 -
                                               config->refresh))))) best_mode =
                    mode;

            // set mode if found
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
        if (config->scale > 0.0)
            wlr_output_state_set_scale(&state,
                                       round(config->scale * 120) / 120);
        else
            wlr_output_state_set_scale(&state, 1);

        // transform
        wlr_output_state_set_transform(&state, config->transform);

        // adaptive sync
        wlr_output_state_set_adaptive_sync_enabled(
            &state, wlr_output->adaptive_sync_supported ? config->adaptive_sync
                                                        : false);

        // render format
        RenderBitDepth render_bit_depth =
            config->render_bit_depth != RENDER_BIT_DEPTH_DEFAULT
                ? config->render_bit_depth
                : (config->hdr ? RENDER_BIT_DEPTH_10 : RENDER_BIT_DEPTH_8);
        if (config->hdr && bit_depth_from_format(wlr_output->render_format) ==
                               RENDER_BIT_DEPTH_10)
            wlr_output_state_set_render_format(&state,
                                               wlr_output->render_format);
        else if (render_bit_depth == RENDER_BIT_DEPTH_10)
            wlr_output_state_set_render_format(&state, DRM_FORMAT_XRGB2101010);
        else if (wlr_output->render_format == RENDER_BIT_DEPTH_6)
            wlr_output_state_set_render_format(&state, DRM_FORMAT_RGB565);
        else if (wlr_output->render_format == RENDER_BIT_DEPTH_8)
            wlr_output_state_set_render_format(&state, DRM_FORMAT_RGB888);
        else
            wlr_output_state_set_render_format(&state, DRM_FORMAT_XRGB8888);

        // hdr
        bool hdr = config->hdr ? supports_hdr() : false;

        // if (config->hdr && config->color_transform) {
        //    wlr_log(WLR_ERROR,
        //            "cannot enable both HDR and color transform on output %s",
        //            wlr_output->name);
        //    config->hdr = false;
        //}
        if (hdr) {
            const wlr_output_image_description image_desc{
                WLR_COLOR_NAMED_PRIMARIES_BT2020,
                WLR_COLOR_TRANSFER_FUNCTION_ST2084_PQ,
                {},
                {},
                {},
                {}};
            wlr_output_state_set_image_description(&state, &image_desc);
        } else if (wlr_output->supported_primaries ||
                   wlr_output->supported_transfer_functions)
            wlr_output_state_set_image_description(&state, nullptr);
    }

    // tearing
    allow_tearing = config->allow_tearing;

    // max render time
    max_render_time = config->max_render_time;

    bool success;
    if (test_only)
        // test and get status
        success = wlr_output_test_state(wlr_output, &state);
    else {
        // commit and get status
        success = wlr_output_commit_state(wlr_output, &state);

        if (success) {
            // update position
            wlr_output_layout_add(server->output_manager->layout, wlr_output,
                                  static_cast<int>(config->x),
                                  static_cast<int>(config->y));

            // rearrange
            arrange_layers();
        }
    }

    wlr_output_state_finish(&state);
    return success;
}
