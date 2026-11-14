# this file is part of elaro

if want_gregex
  regex_impl = files('iregex-gregex.c')
else
  regex_impl = files('iregex-regexh.c')
endif

if have_capsicum
  core_capsicum_source = files('capsicum.c')
else
  core_capsicum_source = []
endif

libcore_a = static_library('core',
  files(
    'args.c',
    'channels-setup.c',
    'channels.c',
    'chat-commands.c',
    'chat-protocols.c',
    'chatnets.c',
    'commands.c',
    'core.c',
    'expandos.c',
    'ignore.c',
    'levels.c',
    'line-split.c',
    'log-away.c',
    'log.c',
    'masks.c',
    'misc.c',
    'modules-load.c',
    'modules.c',
    'net-disconnect.c',
    'net-nonblock.c',
    'net-sendbuffer.c',
    'network-openssl.c',
    'network.c',
    'nicklist.c',
    'nickmatch-cache.c',
    'pidwait.c',
    'queries.c',
    'rawlog.c',
    'recode.c',
    'refstrings.c',
    'servers-reconnect.c',
    'servers-setup.c',
    'servers.c',
    'session.c',
    'settings.c',
    'signals.c',
    'special-vars.c',
    'tls.c',
    'utf8.c',
    'wcwidth-wrapper.c',
    'wcwidth.c',
    'write-buffer.c',
  )
  + core_capsicum_source
  + regex_impl
  + [
    elaro_version_h,
  ],
  include_directories : rootinc,
  implicit_include_directories : false,
  c_args : [
    def_moduledir,
    def_sysconfdir,
  ],
  dependencies : dep)

