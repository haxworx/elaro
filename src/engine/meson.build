threads_dep = dependency('threads')
engine_lib = static_library('irc-engine-adapter', 'elaro_engine.c', 'elaro_list.c', 'elaro_bans.c', 'elaro_dcc.c', 'elaro_ctcp.c',
  include_directories : rootinc,
  dependencies : dep + [threads_dep, intl_dep])
