# awm

### Supported protocols
- `xdg-shell`
- `ext-foreign-toplevel-list-v1`
- `ext-image-capture-source-v1`
- `ext-image-copy-capture-v1`
- `xdg-output-unstable-v1`
- `wlr-output-management-unstable-v1`
- `wlr-layer-shell-unstable-v1`
- `wlr-screencopy-unstable-v1`
- `wlr-data-control-unstable-v1`
- `wlr-gamma-control-unstable-v1`

### Dependencies
- `wlroots` **0.19** (wlroots-git)
- `xkbcommon`
- `wayland-server`
- `wayland-protocols`
- `tomlcpp` (submodule)

### Build
You can build and run using:
```sh
# clone
git clone https://github.com/dy-tea/awm.git --recurse-submodules

# build
meson setup build
ninja -C build

# run
./build/awm
```
Additionally, you can install awm to your wayland-sessions using:
```sh
sudo meson install -C build
````
