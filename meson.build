project(
  'awm',
  'cpp',
  'c',
  version: '0.5.1',
  license: 'GPL-3.0-or-later',
  default_options: [
    'warning_level=3',
    'optimization=3',
    'buildtype=release',
    'cpp_std=c++17',
    'debug=true',
  ],
  meson_version: '>= 1.2.0',
)

# optional xwayland support
xwayland_option = get_option('XWAYLAND')
if xwayland_option
  xwayland = dependency('xwayland', required: xwayland_option)
  add_project_arguments('-DXWAYLAND', language: 'cpp')
endif

# optional systemd support
systemd_option = get_option('SYSTEMD')
if systemd_option
  add_project_arguments('-DSYSTEMD', language: 'cpp')
endif

# programs
wayland_scanner = find_program('wayland-scanner')

# wayland protocols
protocol_sources = []
protocols = [
  'protocols' / 'wlr-layer-shell-unstable-v1.xml',
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
endforeach

# dependencies
tomlcpp_proj = subproject('tomlcpp')
tomlcpp_dep = tomlcpp_proj.get_variable('tomlcpp_dep')
json = dependency('nlohmann_json')

# wlroots
wlroots_version = ['>=0.20.0', '<0.21.0']
if false
  # this looks weird but it will use the wlroots in the subprojects directory
  # if system wlroots is not found
  subproject(
    'wlroots',
    default_options: ['examples=false'],
    required: false,
    version: wlroots_version,
  )
endif
wlroots = dependency('wlroots-0.20', version: wlroots_version, fallback: 'wlroots')

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
  wlroots,
  json,
]

# xwayland
if xwayland_option
  libs += [xwayland]
endif

# backward-cpp
backward_option = get_option('backward')
if backward_option
  backward_proj = subproject('backward-cpp', default_options: {'tests': false})
  backward = backward_proj.get_variable('backward_dep')
  backward_interface = backward_proj.get_variable('backward_interface_dep')
  add_project_arguments('-DBACKWARD', language: 'cpp')
  libs += [backward, backward_interface]
endif

# main executable
executable(
  'awm',
  [
    'src' / 'main.cpp',
    'src' / 'Server.cpp',
    'src' / 'Keyboard.cpp',
    'src' / 'Toplevel.cpp',
    'src' / 'Output.cpp',
    'src' / 'Popup.cpp',
    'src' / 'LayerSurface.cpp',
    'src' / 'Workspace.cpp',
    'src' / 'Config.cpp',
    'src' / 'Cursor.cpp',
    'src' / 'OutputManager.cpp',
    'src' / 'PointerConstraint.cpp',
    'src' / 'SessionLock.cpp',
    'src' / 'TextInput.cpp',
    'src' / 'IPC.cpp',
    'src' / 'Seat.cpp',
    'src' / 'Decoration.cpp',
    'src' / 'WindowRule.cpp',
    'src' / 'WorkspaceManager.cpp',
    'src' / 'IdleInhibitor.cpp',
    'src' / 'InputRelay.cpp',
    'src' / 'InputMethod.cpp',
    'src' / 'InputMethodPopup.cpp',
    protocol_sources,
  ],
  include_directories: include,
  dependencies: libs,
  install: true,
  install_dir: get_option('bindir'),
)

# desktop file
install_data('awm.desktop', install_dir: '/usr/share/wayland-sessions')

# portals file
install_data(
  'awm-portals.conf',
  install_dir: get_option('datadir') / 'xdg-desktop-portal',
)

# ipc client
executable(
  'awmsg',
  'awmsg/main.cpp',
  dependencies: json,
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
    'workspaces_misc.cpp',
  ]

  foreach t : tests
    name = 't_@0@'.format(t.strip('.cpp'))
    test = executable(
      name,
      ['awmtest' / 'tests' / t],
      dependencies: json,
    )
    test(name, test, is_parallel: false, timeout: 0)
  endforeach
endif
