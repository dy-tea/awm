project('compositor', 'cpp',
  default_options: [
    'cpp_std=c++17',
  ],
)

wayland_scanner = find_program('wayland-scanner')
pkg_config = find_program('pkg-config')

protocols_stable = [
    'xdg-shell'
]
protocols_unstable = []
protocol_sources = []

foreach protocol : protocols_stable
    prefix = run_command(pkg_config, '--variable=pkgdatadir', 'wayland-protocols')
    path = prefix.stdout().replace('\n', '') / 'stable' / protocol / '@0@.xml'.format(protocol)

    out_h = '@0@-protocol.h'.format(protocol)

    header = custom_target(
        out_h,
        output: out_h,
        input: path,
        command: [ wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@' ],
    )

    protocol_sources += [header]
endforeach

foreach protocol : protocols_unstable
    prefix = run_command(pkg_config, '--variable=pkgdatadir', 'wayland-protocols')
    name = protocol.replace('-unstable-v.', '')
    path = prefix.stdout().replace('\n', '') / 'unstable' / name / '@0@.xml'.format(protocol)

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
  dependency('wlroots-0.19'),
  dependency('pixman-1'),
  dependency('xkbcommon')
]

executable(
  'compositor',
  ['src/main.cpp', 'src/Server.cpp', protocol_sources],
  cpp_args: ['-fpermissive'],
  include_directories: include,
  dependencies: libs
)
