# HexChat GTK4/libadwaita frontend

gtk4_dep = dependency('gtk4', version: '>= 4.10')
adw_dep = dependency('libadwaita-1', version: '>= 1.4')

hexchat_gtk4_sources = [
  'fe-gtk4.c',
  'chanlist.c',
  'servlistgui.c',
  'prefs.c',
]

hexchat_gtk4_deps = [
  hexchat_common_dep,
  libgmodule_dep,
  gtk4_dep,
  adw_dep,
]

hexchat_gtk4_cflags = []

executable('hexchat-gtk4',
  sources: hexchat_gtk4_sources,
  dependencies: hexchat_gtk4_deps,
  c_args: hexchat_gtk4_cflags,
  pie: true,
  install: true,
  gui_app: true,
)
