#!/bin/sh

# This script generates the xdg-shell protocol files from the xml file
wayland-scanner server-header $(pkg-config --variable=pkgdatadir wayland-protocols)/stable/xdg-shell/xdg-shell.xml include/xdg-shell-protocol.h

if [ -f include/xdg-shell-protocol.h ]; then
    echo "File generated successfully, output to include/xdg-shell-protocol.h"
else
    echo "File not generated, ensure that you have wayland-scanner available in \$PATH"
fi
