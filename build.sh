#!/bin/bash

# remove build directory if exists
if [ -d "build" ]; then
    rm -rf build
fi

# remove executable files
rm -f awm awm_xwayland

# build with xwayland support
meson setup build -DXWAYLAND=true
ninja -C build
cp build/awm awm_xwayland

# build without xwayland support
meson configure build -DXWAYLAND=false
ninja -C build
cp build/awm .

# print version
./awm -v
