project(
  'awm',
  'cpp',
  'c',
  default_options: [
    'cpp_std=c++17',
  ],
)

xwayland = dependency('xwayland', required: get_option('XWAYLAND'))

if xwayland.found()
  add_project_arguments('-DXWAYLAND', language: 'cpp')
endif

wayland_scanner = find_program('wayland-scanner')
pkg_config = find_program('pkg-config')

protocol_sources = []
protocol_code = []

wl_protocols_dir = run_command(pkg_config, '--variable=pkgdatadir', 'wayland-protocols').stdout().replace('\n', '')

protocols = [
  wl_protocols_dir / 'stable' / 'xdg-shell' / 'xdg-shell.xml',
  wl_protocols_dir / 'stable' / 'tablet' / 'tablet-v2.xml',
  wl_protocols_dir / 'staging' / 'ext-foreign-toplevel-list' / 'ext-foreign-toplevel-list-v1.xml',
  wl_protocols_dir / 'staging' / 'ext-image-capture-source' / 'ext-image-capture-source-v1.xml',
  wl_protocols_dir / 'staging' / 'ext-image-copy-capture' / 'ext-image-copy-capture-v1.xml',
  wl_protocols_dir / 'staging' / 'cursor-shape' / 'cursor-shape-v1.xml',
  wl_protocols_dir / 'unstable' / 'xdg-output' / 'xdg-output-unstable-v1.xml',
  wl_protocols_dir / 'unstable' / 'linux-dmabuf' / 'linux-dmabuf-unstable-v1.xml',
  wl_protocols_dir / 'unstable' / 'pointer-constraints' / 'pointer-constraints-unstable-v1.xml',
  'protocols' / 'wlr-layer-shell-unstable-v1.xml',
  'protocols' / 'wlr-data-control-unstable-v1.xml',
  'protocols' / 'wlr-screencopy-unstable-v1.xml',
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

tomlcpp_proj = subproject('tomlcpp')
tomlcpp_dep = tomlcpp_proj.get_variable('tomlcpp_dep')

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
  dependency('libinput'),
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
    'src/LayerSurface.cpp',
    'src/Workspace.cpp',
    'src/Config.cpp',
    'src/Cursor.cpp',
    'src/OutputManager.cpp',
    'src/PointerConstraint.cpp',
    'src/SessionLock.cpp',
    'src/IPC.cpp',
    protocol_sources,
    protocol_code,
  ],
  include_directories: include,
  dependencies: libs,
  install: true,
  install_dir: get_option('bindir'),
)

install_data('awm.desktop', install_dir: '//usr/share/wayland-sessions')
