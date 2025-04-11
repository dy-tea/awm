project(
  'awm',
  'cpp',
  'c',
  version: '0.1.0',
  license: 'GPL-3.0-or-later',
  default_options: [
    'warning_level=3',
    'optimization=3',
    'buildtype=release',
    'cpp_std=c++17',
  ],
  meson_version: '>= 1.1',
)

# optional xwayland support
xwayland_option = get_option('XWAYLAND')
if xwayland_option
  xwayland = dependency('xwayland', required: xwayland_option)
  add_project_arguments('-DXWAYLAND', language: 'cpp')
endif

# programs
wayland_scanner = find_program('wayland-scanner')

# wayland dirs
wl_protocols_dep = dependency('wayland-protocols', version: '>=1.41', default_options: ['tests=false'])
wl_protocols_dir = wl_protocols_dep.get_variable(pkgconfig: 'pkgdatadir')

protocol_sources = []
protocol_code = []

# wayland protocols
protocols = [
  wl_protocols_dir / 'stable' / 'xdg-shell' / 'xdg-shell.xml',
  wl_protocols_dir / 'stable' / 'tablet' / 'tablet-v2.xml',
  wl_protocols_dir / 'staging' / 'ext-foreign-toplevel-list' / 'ext-foreign-toplevel-list-v1.xml',
  wl_protocols_dir / 'staging' / 'ext-image-capture-source' / 'ext-image-capture-source-v1.xml',
  wl_protocols_dir / 'staging' / 'ext-image-copy-capture' / 'ext-image-copy-capture-v1.xml',
  wl_protocols_dir / 'staging' / 'cursor-shape' / 'cursor-shape-v1.xml',
  wl_protocols_dir / 'staging' / 'color-management' / 'color-management-v1.xml',
  wl_protocols_dir / 'unstable' / 'xdg-output' / 'xdg-output-unstable-v1.xml',
  wl_protocols_dir / 'unstable' / 'linux-dmabuf' / 'linux-dmabuf-unstable-v1.xml',
  wl_protocols_dir / 'unstable' / 'pointer-constraints' / 'pointer-constraints-unstable-v1.xml',
  'protocols' / 'wlr-layer-shell-unstable-v1.xml',
  'protocols' / 'wlr-data-control-unstable-v1.xml',
  'protocols' / 'wlr-screencopy-unstable-v1.xml',
  'protocols' / 'wlr-output-power-management-unstable-v1.xml',
]

# generate protocol headers and code
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

# dependencies
tomlcpp_proj = subproject('tomlcpp')
tomlcpp_dep = tomlcpp_proj.get_variable('tomlcpp_dep')
nlohmann_json = dependency('nlohmann_json')

wlroots_version = ['>=0.19.0', '<0.20.0']
if get_option('wlroots')
  wlroots = subproject(
    'wlroots',
    default_options: ['examples=false'],
    required: false,
    version: wlroots_version,
  ).get_variable('wlroots')
endif
wlroots = dependency('wlroots-0.19', version: wlroots_version, fallback: 'wlroots')

# includes
include = include_directories(
  'include',
)

# libs
libs = [
  dependency('wayland-server'),
  dependency('pixman-1'),
  dependency('xkbcommon'),
  dependency('libinput'),
  dependency('xcb'),
  tomlcpp_dep,
  nlohmann_json,
  wlroots,
]

# xwayland
if xwayland_option
  libs += [xwayland]
endif

# main executable
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
    'src/TextInput.cpp',
    'src/IPC.cpp',
    'src/Seat.cpp',
    'src/ServerDecoration.cpp',
    protocol_sources,
    protocol_code,
  ],
  include_directories: include,
  dependencies: libs,
  install: true,
  install_dir: get_option('bindir'),
)

# desktop file
install_data('awm.desktop', install_dir: '/usr/share/wayland-sessions')

# ipc client
executable(
  'awmsg',
  'awmsg/main.cpp',
  dependencies: nlohmann_json,
  install: true,
  install_dir: get_option('bindir'),
)

# completion scripts
install_data(
  'awmsg/awmsg.bash',
  install_dir: get_option('datadir') / 'bash-completion' / 'completions',
  install_tag: 'runtine',
  rename: 'awmsg',
)

install_data(
  'awmsg/awmsg.fish',
  install_dir: get_option('datadir') / 'fish' / 'vendor_completions.d',
  install_tag: 'runtine',
)

install_data(
  'awmsg/awmsg.zsh',
  install_dir: get_option('datadir') / 'zsh' / 'site-functions',
  install_tag: 'runtime',
  rename: '_awmsg',
)

# tests
if get_option('tests')
  # dependencies
  find_program('alacritty')
  find_program('tmatrix')
  find_program('waybar')

  tests = [
    'fullscreen_10.cpp',
    'fullscreen_size.cpp',
    'maximize_10.cpp',
    'maximize_size.cpp',
    'maximize_waybar.cpp',
    'workspaces_10.cpp',
  ]

  foreach t : tests
    name = 'test_@0@'.format(t.strip('.cpp'))
    test = executable(
      name,
      ['awmtest' / 'tests' / t],
      dependencies: nlohmann_json,
    )
    test(name, test, is_parallel: false, timeout: 0)
  endforeach
endif
