# awm

### Supported protocols
- `xdg-shell`
- `xdg-output-unstable-v1`
- `wlr-layer-shell-unstable-v1`
- `wlr-screencopy-unstable-v1`

### Dependencies
- `wlroots` **0.19** (wlroots-git)
- `xkbcommon`
- `wayland-server`
- `wayland-protocols`
- `pixman`

### Build
You can build and run using:
```sh
meson setup build
ninja -C build
./build/awm # to run
```
Addtionally, you can install awm to your wayland-sessions using:
```sh
meson setup build
sudo meson install -C build
````
