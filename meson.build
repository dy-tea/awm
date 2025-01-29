project(
    'awm',
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
protocol_code = []

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

    out_hc = '@0@@-client-protocol.h'.format(protocol)
    client_header = custom_target(
        out_hc,
        output: out_hc,
        input: path,
        command: [ wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@' ],
    )

    out_c = '@0@-protocol.c'.format(protocol)
    code = custom_target(
        out_c,
        output: out_c,
        input: path,
        command: [ wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@' ],
    )
    protocol_code += [code]
endforeach

protocols_wlr_unstable = [
    'wlr-layer-shell-unstable-v1',
    'wlr-foreign-toplevel-management-unstable-v1'
]

foreach protocol : protocols_wlr_unstable
    path = 'protocols' / '@0@.xml'.format(protocol)

    out_h = '@0@-protocol.h'.format(protocol)
    header = custom_target(
        out_h,
        output: out_h,
        input: path,
        command: [ wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@' ]
    )
    protocol_sources += [header]

    out_hc = '@0@@-client-protocol.h'.format(protocol)
    client_header = custom_target(
        out_hc,
        output: out_hc,
        input: path,
        command: [ wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@' ],
    )

    out_c = '@0@-protocol.c'.format(protocol)
    code = custom_target(
        out_c,
        output: out_c,
        input: path,
        command: [ wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@' ]
    )
    protocol_code += [code]
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
    'awm',
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
        protocol_code,
    ],
    cpp_args: ['-fpermissive'],
    include_directories: include,
    dependencies: libs,
)
