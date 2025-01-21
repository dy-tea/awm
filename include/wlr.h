#pragma once

#include <EGL/egl.h>
#include <errno.h>
#include <libinput.h>
#include <libudev.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <wayland-client.h>
#include <wayland-server.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>
#include <xcb/xcb.h>
#include <xkbcommon/xkbcommon.h>

// Rename fields which use reserved names
#define class class_
#define namespace namespace_
#define delete delete_
#define static
// wlroots does not do this itself
extern "C" {

#define WLR_USE_UNSTABLE 1

#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>

#include <wlr/util/log.h>
#include <wlr/util/region.h>

#ifdef WLR_HAS_XWAYLAND
#include <wlr/xwayland.h>
#endif
}

// make sure to undefine these again
#undef class
#undef namespace
#undef static
#undef delete
