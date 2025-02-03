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
Addtionally, you can install awm to your wayland-sessions using:
```sh
sudo meson install -C build
````
