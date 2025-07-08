# awm

A wayland compositor with high protocol support, simple customization and an IPC server.

[![Build](https://github.com/dy-tea/awm/actions/workflows/build.yml/badge.svg)](https://github.com/dy-tea/awm/actions/workflows/build.yml)

https://github.com/user-attachments/assets/ebbc565e-4f09-4ac7-b204-8dea5832f9e3

### Dependencies

- `meson`
- `ninja`
- `wlroots-git` **0.20**
- `xkbcommon`
- `wayland-server`
- `wayland-protocols`
- `nlohmann-json`
- `xwayland` (can be disabled in build)

### Build

You can build and run using:

```sh
# clone
git clone https://github.com/dy-tea/awm.git --recursive
cd awm/

# build
meson setup build/
ninja -C build/

# run
./build/awm
```

**Install**

```sh
sudo meson install -C build/
```

**Xwayland support**

Currently xwayland support is very unstable and might even be removed in the future, to remove support do:

```sh
meson setup build -DXWAYLAND=false
```

**Custom wlroots**

If system wlroots is not found, clone wlroots to `subprojects/wlroots` and build using that instead of system wlroots. Note that `libwlroots-0.20.so` won't be installed to `/usr/lib` so you will have to copy it there if you want to run awm using the drm backend.

### Configuration

An example configuration can be found in [config-minimal.toml](config-minimal.toml).
You can copy this file to `~/.config/awm/config.toml` and modify it to your liking.
All options are documented in [config.toml](config.toml) and many are explained in the wiki.

### Testing

**Tests**

You can build and run tests using:

```sh
meson setup build/ -Dtests=true
meson test -C build/
```

Note that tests have additional dependencies.

**Backtrace**

If you find a crash and would like to get a backtrace, enable `backward-cpp` traces using:

```sh
meson configure -Dbackward=true build/
ninja -C build/
```

You can then create a log file by running awm with:

```sh
script -c "awm" log.txt
```

### Supported protocols

- [Presentation time](https://wayland.app/protocols/presentation-time)
- [Viewporter](https://wayland.app/protocols/viewporter)
- [XDG shell](https://wayland.app/protocols/xdg-shell)
- [Linux DMA-BUF](https://wayland.app/protocols/linux-dmabuf-v1)
- [XDG activation](https://wayland.app/protocols/xdg-activation-v1)
- [DRM lease](https://wayland.app/protocols/drm-lease-v1)
- [DRM synchronization object](https://wayland.app/protocols/linux-drm-syncobj-v1)
- [Session lock](https://wayland.app/protocols/ext-session-lock-v1)
- [Single-pixel buffer](https://wayland.app/protocols/single-pixel-buffer-v1)
- [Content type hint](https://wayland.app/protocols/content-type-v1)
- [Idle notify](https://wayland.app/protocols/ext-idle-notify-v1)
- [Image Capture Source](https://wayland.app/protocols/ext-image-capture-source-v1)
- [Image Copy Capture](https://wayland.app/protocols/ext-image-copy-capture-v1)
- [Xwayland shell](https://wayland.app/protocols/xwayland-shell-v1) **WIP**
- [Fractional scale](https://wayland.app/protocols/fractional-scale-v1)
- [Cursor shape](https://wayland.app/protocols/cursor-shape-v1)
- [Foreign toplevel list](https://wayland.app/protocols/ext-foreign-toplevel-list-v1)
- [XDG dialog windows](https://wayland.app/protocols/xdg-dialog-v1)
- [Alpha modifier protocol](https://wayland.app/protocols/alpha-modifier-v1)
- [Data control protocol](https://wayland.app/protocols/ext-data-control-v1)
- [XDG System bell](https://wayland.app/protocols/xdg-system-bell-v1)
- [Color management](https://wayland.app/protocols/color-management-v1)
- [Idle inhibit](https://wayland.app/protocols/idle-inhibit-unstable-v1)
- [Keyboard shortcuts inhibit](https://wayland.app/protocols/keyboard-shortcuts-inhibit-unstable-v1)
- [Pointer constraints](https://wayland.app/protocols/pointer-constraints-unstable-v1)
- [Primary Selection](https://wayland.app/protocols/primary-selection-unstable-v1)
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
