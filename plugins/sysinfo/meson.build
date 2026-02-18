sysinfo_sources = [
  'format.c',
  'sysinfo.c',
]

sysinfo_deps = [
  libgio_dep,
  hexchat_plugin_dep,
  common_sysinfo_deps,
]

sysinfo_includes = []
sysinfo_cargs = []

sysinfo_includes += 'shared'
sysinfo_sources += [
  'shared/df.c'
]

libpci = dependency('libpci', required: false, method: 'pkg-config')
if libpci.found()
  sysinfo_deps += libpci
  sysinfo_cargs += '-DHAVE_LIBPCI'
  sysinfo_sources += 'unix/pci.c'

  picidsdir = libpci.get_pkgconfig_variable('idsdir')
  pciids = join_paths(picidsdir, 'pci.ids')
  sysinfo_cargs += '-DPCIIDS_FILE="@0@"'.format(pciids)
endif
sysinfo_includes += 'unix'
sysinfo_sources += [
  'unix/backend.c',
  'unix/match.c',
  'unix/parse.c',
]

shared_module('sysinfo', sysinfo_sources,
  dependencies: sysinfo_deps,
  include_directories: include_directories(sysinfo_includes),
  c_args: sysinfo_cargs,
  install: true,
  install_dir: plugindir,
  name_prefix: '',
)
