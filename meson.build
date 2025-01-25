project('compositor', 'cpp',
  default_options: [
    'cpp_std=c++17',
  ],
)

wayland_scanner = find_program('wayland-scanner')
hyprwayland_scanner = find_program('hyprwayland-scanner')
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
    protocol_sources
  ],
  cpp_args: ['-fpermissive'],
  include_directories: include,
  dependencies: libs
)
