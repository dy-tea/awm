project('compositor', 'cpp',
  default_options: [
    'cpp_std=c++17',
  ],
)

include = include_directories(
  '/usr/include',
  '/usr/include/wlroots-0.19',
  'include'
)

libs = [
  dependency('wayland-server'),
  dependency('wlroots-0.19'),
  dependency('pixman-1'),
  dependency('xkbcommon')
]

executable(
  'compositor',
  ['src/main.cpp', 'src/Server.cpp'],
  include_directories: include,
  dependencies: libs
)
