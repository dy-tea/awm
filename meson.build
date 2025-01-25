project(
    'compositor',
    'cpp',
    'c',
    default_options: [
        'cpp_std=c++17',
    ],
)
add_global_arguments('-DWLR_USE_UNSTABLE=1', language: 'c')

wayland_scanner = find_program('wayland-scanner')
pkg_config = find_program('pkg-config')

protocol_sources = []

protocols_wl_stable = [
    'xdg-shell'
]

wl_protocols_dir = run_command(pkg_config, '--variable=pkgdatadir', 'wayland-protocols').stdout().replace('\n', '')
foreach protocol : protocols_wl_stable
    path = wl_protocols_dir / 'stable' / protocol / '@0@.xml'.format(protocol)

    out_h = '@0@-protocol.h'.format(protocol)

    header = custom_target(
        out_h,
        output: out_h,
        input: path,
        command: [ wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@' ],
    )

    protocol_sources += [header]
endforeach

protocols_wlr_unstable = [
    'wlr-layer-shell-unstable-v1'
]

wlr_protocols_dir = run_command(pkg_config, '--variable=pkgdatadir', 'wlr-protocols').stdout().replace('\n', '')
foreach protocol : protocols_wlr_unstable
    path = wlr_protocols_dir / 'unstable' / '@0@.xml'.format(protocol)

    out_h = '@0@-protocol.h'.format(protocol)

    header = custom_target(
        out_h,
        output: out_h,
        input: path,
        command: [ wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@' ]
    )

    protocol_sources += [header]
endforeach

include = include_directories(
  '/usr/include',
  '/usr/include/wlroots-0.19',
  'include',
)

libs = [
    dependency('wayland-server'),
    dependency('wayland-protocols'),
    dependency('wlroots-0.19'),
    dependency('pixman-1'),
    dependency('xkbcommon')
]

executable(
    'compositor',
    [
        'src/main.cpp',
        'src/Server.cpp',
        'src/Keyboard.cpp',
        'src/Toplevel.cpp',
        'src/Output.cpp',
        'src/Popup.cpp',
        'src/LayerShell.cpp',
        'src/LayerSurface.cpp',
        protocol_sources,
    ],
    cpp_args: ['-fpermissive'],
    include_directories: include,
    dependencies: libs
)
