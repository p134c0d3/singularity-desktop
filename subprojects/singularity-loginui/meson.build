project('singularity-loginui', 'c',
  version: '0.1.0',
  meson_version: '>= 0.59.0',
)

loginui_deps = [
  dependency('cairo'),
  dependency('pangocairo'),
  dependency('gdk-pixbuf-2.0'),
  dependency('wayland-client'),
]

loginui_lib = static_library('loginui',
  'loginui.c',
  dependencies: loginui_deps,
  install: false,
)

loginui_dep = declare_dependency(
  link_with: loginui_lib,
  include_directories: include_directories('.'),
  dependencies: loginui_deps,
)
