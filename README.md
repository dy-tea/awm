# awm

### Supported protocols

- [Presentation time](https://wayland.app/protocols/presentation-time)
- [Viewporter](https://wayland.app/protocols/viewporter)
- [XDG shell](https://wayland.app/protocols/xdg-shell)
- [Linux DMA-BUF](https://wayland.app/protocols/linux-dmabuf-v1)
- [DRM synchronization object](https://wayland.app/protocols/linux-drm-syncobj-v1)
- [Session lock](https://wayland.app/protocols/ext-session-lock-v1)
- [Single-pixel buffer](https://wayland.app/protocols/single-pixel-buffer-v1)
- [Idle notify](https://wayland.app/protocols/ext-idle-notify-v1)
- [Image Capture Source](https://wayland.app/protocols/ext-image-capture-source-v1)
- [Image Copy Capture](https://wayland.app/protocols/ext-image-copy-capture-v1)
- [Xwayland shell](https://wayland.app/protocols/xwayland-shell-v1) **WIP**
- [Fractional scale](https://wayland.app/protocols/fractional-scale-v1)
- [Cursor shape](https://wayland.app/protocols/cursor-shape-v1)
- [Foreign toplevel list](https://wayland.app/protocols/ext-foreign-toplevel-list-v1)
- [Alpha modifier protocol](https://wayland.app/protocols/alpha-modifier-v1)
- [Data control protocol](https://wayland.app/protocols/ext-data-control-v1)
- [Pointer constraints](https://wayland.app/protocols/pointer-constraints-unstable-v1)
- [Relative pointer](https://wayland.app/protocols/relative-pointer-unstable-v1)
- [XDG output](https://wayland.app/protocols/xdg-output-unstable-v1)
- [wlr data control](https://wayland.app/protocols/wlr-data-control-unstable-v1)
- [wlr export DMA-BUF](https://wayland.app/protocols/wlr-export-dmabuf-unstable-v1)
- [wlr foreign toplevel management](https://wayland.app/protocols/wlr-foreign-toplevel-management-unstable-v1)
- [wlr gamma control](https://wayland.app/protocols/wlr-gamma-control-unstable-v1)
- [wlr layer shell](https://wayland.app/protocols/wlr-layer-shell-unstable-v1)
- [wlr output management](https://wayland.app/protocols/wlr-output-management-unstable-v1)
- [wlr output power management](https://wayland.app/protocols/wlr-output-power-management-unstable-v1)
- [wlr screencopy](https://wayland.app/protocols/wlr-screencopy-unstable-v1)
- [wlr virtual pointer](https://wayland.app/protocols/wlr-virtual-pointer-unstable-v1)
- [Virtual keyboard](https://wayland.app/protocols/virtual-keyboard-unstable-v1)

### Dependencies

- `meson`
- `ninja`
- `wlroots` **0.19** (wlroots-git)
- `xkbcommon`
- `wayland-server`
- `wayland-protocols`
- `xwayland` (can be disabled in build)

### Build

You can build and run using:

```sh
# clone
git clone https://github.com/dy-tea/awm.git
cd awm/

# build
meson setup build/
ninja -C build/

# run
./build/awm
```

Additionally, you can install awm to your wayland-sessions using:

```sh
sudo meson install -C build/
```

### Configuration

An example configuration can be found in [config.toml](config.toml).
You can copy this file to `~/.config/awm/config.toml` and modify it to your liking.

### FIXME

Current issues that need fixing:

- pixman renderer is not working, crash cites drm syncobj
- session lock appears to be broken (tested with swaylock and hyprlock)
- workspace list run with continuous flag does not update on non-empty workspaces
