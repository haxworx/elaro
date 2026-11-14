elementary_dep = declare_dependency(dependencies : [
  dependency('elementary', version : '>=1.26'), intl_dep])
eet_dep = dependency('eet')
ecore_file_dep = dependency('ecore-file')
efreet_dep = dependency('efreet')
frontend_support = static_library('efl-support', 'elaro_util.c',
  dependencies : [ecore_file_dep, dep[2]])
config_lib = static_library('connection-config', 'elaro_config.c',
  include_directories : rootinc,
  link_with : frontend_support,
  dependencies : [eet_dep, ecore_file_dep, intl_dep])
subdir('ui')
elaro_efl = executable('elaro', 'elaro_main.c', 'elaro_music_backend.c', ui_sources,
  include_directories : rootinc,
  link_with : [config_lib, engine_lib, libconfig_a, libcore_a, libirc_a],
  dependencies : [elementary_dep, eet_dep, ecore_file_dep, efreet_dep],
  c_args : ['-DELARO_BUILD_DIR="' + meson.project_build_root() + '"'],
  link_depends : elaro_assets,
  export_dynamic : true,
  install : true)
