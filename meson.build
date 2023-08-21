project('tinytile', 'c',
  version: '0.3.0',
  license: 'BSD-3-Clause',
  meson_version: '>=0.59.0',
  default_options: [
    'c_std=c11',
    'warning_level=2',
    'werror=true',
  ],
)

add_project_arguments(
  [
    '-DWLR_USE_UNSTABLE',
    '-Wundef',
    '-Wno-unused-parameter',
  ],
  language: 'c',
)

if get_option('buildtype').startswith('debug')
  add_project_arguments('-DDEBUG', language : 'c')
endif

wl_protocol_dir = dependency('wayland-protocols', version: '>=1.14').get_variable('pkgdatadir')
protocols_being_used = [
  [wl_protocol_dir, 'stable/xdg-shell/xdg-shell.xml'],
]

headers_for_protocols_being_used = []
foreach protocol : protocols_being_used
  xml = join_paths(protocol)
  headers_for_protocols_being_used += generator(
    find_program('wayland-scanner'),
    output: '@BASENAME@-protocol.h',
    arguments: ['server-header', '@INPUT@', '@OUTPUT@'],
  ).process(xml)
endforeach

executable(
  meson.project_name(),
  'main.c',
  dependencies: [
    dependency('wlroots', version: '>= 0.16.0', fallback: ['wlroots', 'wlroots']),
    dependency('wayland-server'),
    dependency('xkbcommon'),
    dependency('libinput'),
    declare_dependency(
      sources: headers_for_protocols_being_used,
    ),
  ],
  install: true,
)