project(
  'awm',
  'cpp',
  'c',
  default_options: [
    'cpp_std=c++17',
  ],
)
add_project_arguments('-Ofast', '-g3', language: 'cpp')
add_global_arguments('-DWLR_USE_UNSTABLE=1', language: 'c')

wayland_scanner = find_program('wayland-scanner')
pkg_config = find_program('pkg-config')

protocol_sources = []
protocol_code = []

wl_protocols_dir = run_command(pkg_config, '--variable=pkgdatadir', 'wayland-protocols').stdout().replace('\n', '')

protocols = [
  wl_protocols_dir / 'stable' / 'xdg-shell' / 'xdg-shell.xml',
  wl_protocols_dir / 'staging' / 'ext-foreign-toplevel-list' / 'ext-foreign-toplevel-list-v1.xml',
  wl_protocols_dir / 'staging' / 'ext-image-capture-source' / 'ext-image-capture-source-v1.xml',
  wl_protocols_dir / 'staging' / 'ext-image-copy-capture' / 'ext-image-copy-capture-v1.xml',
  wl_protocols_dir / 'unstable' / 'xdg-output' / 'xdg-output-unstable-v1.xml',
  'protocols' / 'wlr-layer-shell-unstable-v1.xml',
  'protocols' / 'wlr-data-control-unstable-v1.xml',
]

foreach protocol : protocols
  out_h = '@0@-protocol.h'.format(protocol)
  header = custom_target(
    out_h.underscorify() + '_h',
    input: protocol,
    output: '@BASENAME@-protocol.h',
    command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@'],
  )
  protocol_sources += [header]

  out_c = '@0@-protocol.c'.format(protocol)
  code = custom_target(
    out_c.underscorify() + '_c',
    input: protocol,
    output: '@BASENAME@-protocol.c',
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
  )
  protocol_code += [code]
endforeach

include_dir = run_command(pkg_config, '--variable=includedir', 'wlroots-0.19').stdout().replace('\n', '')

tomlcppp_proj = subproject('tomlcpp')
tomlcpp_dep = tomlcppp_proj.get_variable('tomlcpp_dep')

include = include_directories(
  include_dir,
  include_dir / 'wlroots-0.19',
  'include',
)

libs = [
  dependency('wayland-server'),
  dependency('wayland-protocols'),
  dependency('wlroots-0.19'),
  dependency('pixman-1'),
  dependency('xkbcommon'),
  dependency('xcb'),
  tomlcpp_dep,
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
    'src/Workspace.cpp',
    'src/Config.cpp',
    'src/Cursor.cpp',
    #'src/XWaylandShell.cpp',
    #'src/XWaylandSurface.cpp',
    protocol_sources,
    protocol_code,
  ],
  cpp_args: ['-fpermissive'],
  include_directories: include,
  dependencies: libs,
  install: true,
  install_dir: get_option('bindir'),
)

datadir = get_option('datadir')

install_data('awm.desktop', install_dir: '//usr/share/wayland-sessions')

install_data('config.toml', install_dir: join_paths(datadir, 'awm'))
