engine_test = executable('test-efl-engine', 'test-engine.c',
  include_directories : rootinc,
  link_with : [engine_lib, libconfig_a, libcore_a, libirc_a],
  dependencies : dep + [threads_dep],
  export_dynamic : true)
python = find_program('python3')
notify_sound_test = executable('test-notify-sound', 'test-notify-sound.c',
  '../../src/fe-efl/ui/elaro_ui_notify.c', '../../src/fe-efl/ui/elaro_ui_text.c',
  include_directories : rootinc,
  link_with : frontend_support,
  link_depends : elaro_assets,
  dependencies : [elementary_dep, eet_dep, ecore_file_dep])
test('Private message sound archive', python,
  args : [files('isolated.py'), notify_sound_test], timeout : 15,
  env : ['ELM_ENGINE=buffer', 'ECORE_NO_SYSTEM_MODULES=1', 'EINA_LOG_LEVEL=0'])
test('EFL engine round trip', python,
  args : [files('roundtrip.py'), engine_test], timeout : 90)

test('EFL engine early shutdown', python,
  args : [files('isolated.py'), engine_test, '--stop-immediately'], timeout : 10)
test('Elementary startup and shutdown', python,
  args : [files('isolated.py'), elaro_efl, '--smoke-test'], timeout : 15,
  env : ['ELM_ENGINE=buffer', 'ECORE_NO_SYSTEM_MODULES=1', 'EINA_LOG_LEVEL=0', 'ELM_AUTO_NORENDER_WITHDRAWN=0'])

config_test = executable('test-connection-config', 'test-config.c',
  include_directories : rootinc,
  link_with : config_lib,
  dependencies : [dep[0], eet_dep, ecore_file_dep])
test('Eet connection configuration', python,
  args : [files('isolated.py'), config_test], timeout : 15)

allocation_failure_test = executable('test-allocation-failure', 'test-allocation-failure.c',
  include_directories : rootinc,
  link_with : frontend_support,
  dependencies : [eet_dep, ecore_file_dep, intl_dep])
test('Configuration allocation failures', allocation_failure_test,
  env : ['EINA_LOG_LEVEL=0', 'EINA_LOG_BACKTRACE=0'])
test('IRC SASL authentication', python,
  args : [files('roundtrip.py'), engine_test, '--sasl'], timeout : 90)

transfer_test = executable('test-transfer', 'test-transfer.c',
  include_directories : rootinc,
  link_with : [engine_lib, libconfig_a, libcore_a, libirc_a],
  dependencies : dep + [threads_dep],
  export_dynamic : true)
test('DCC file transfers', python,
  args : [files('transfer.py'), transfer_test], timeout : 30)

send_test = executable('test-send', 'test-send.c',
  include_directories : rootinc,
  link_with : [engine_lib, libconfig_a, libcore_a, libirc_a],
  dependencies : dep + [threads_dep],
  export_dynamic : true)
test('DCC send files', python,
  args : [files('send.py'), send_test], timeout : 30)

irc_text_test = executable('test-irc-text', 'test-irc-text.c',
  '../../src/fe-efl/ui/elaro_ui_text.c',
  link_with : frontend_support,
  include_directories : rootinc,
  dependencies : [dep[0], elementary_dep, ecore_file_dep])
test('IRC text formatting', irc_text_test)

music_ui_test = executable('test-music-ui', 'test-music-ui.c',
  '../../src/fe-efl/ui/elaro_ui_music.c', '../../src/fe-efl/ui/elaro_ui_util.c',
  link_with : frontend_support,
  include_directories : rootinc,
  dependencies : [dep[0], elementary_dep, ecore_file_dep])
test('Music popup crash isolation', python,
  args : [files('isolated.py'), music_ui_test, elaro_efl], timeout : 20,
  env : ['ELM_ENGINE=buffer', 'ECORE_NO_SYSTEM_MODULES=1', 'EINA_LOG_LEVEL=0',
         'ELM_AUTO_NORENDER_WITHDRAWN=0'])
test('Music playback helper', python,
  args : [files('music.py'), elaro_efl], timeout : 40)

links_test = executable('test-links', 'test-links.c',
  '../../src/fe-efl/ui/elaro_ui_util.c',
  include_directories : rootinc,
  link_with : config_lib,
  dependencies : [dep[0], elementary_dep, eet_dep, ecore_file_dep])
test('External link confirmation', python,
  args : [files('isolated.py'), links_test], timeout : 15,
  env : ['ELM_ENGINE=buffer', 'ECORE_NO_SYSTEM_MODULES=1', 'EINA_LOG_LEVEL=0'])

ecore_dep = dependency('ecore')
efl_util_test = executable('test-efl-util', 'test-efl-util.c',
  include_directories : rootinc,
  link_with : frontend_support,
  dependencies : [ecore_file_dep, ecore_dep])
test('EFL strings and process arguments', efl_util_test, timeout : 10,
  env : ['ECORE_NO_SYSTEM_MODULES=1', 'EINA_LOG_LEVEL=0'])

ui_state_test = executable('test-ui-state', 'test-ui-state.c',
  '../../src/engine/elaro_command.c',
  '../../src/fe-efl/ui/elaro_ui_buffers.c', '../../src/fe-efl/ui/elaro_ui_users.c',
  '../../src/fe-efl/ui/elaro_ui_avatar.c', '../../src/fe-efl/ui/elaro_ui_history.c',
  '../../src/fe-efl/ui/elaro_ui_util.c', '../../src/fe-efl/ui/elaro_ui_text.c',
  include_directories : rootinc,
  link_with : frontend_support,
  dependencies : [elementary_dep, ecore_file_dep])
test('EFL conversations and user state', python,
  args : [files('isolated.py'), ui_state_test], timeout : 15,
  env : ['ELM_ENGINE=buffer', 'ECORE_NO_SYSTEM_MODULES=1', 'EINA_LOG_LEVEL=0'])

engine_defaults_test = executable('test-engine-defaults', 'test-engine-defaults.c',
  include_directories : rootinc,
  link_with : [libconfig_a, libcore_a, libirc_a],
  dependencies : dep,
  export_dynamic : true)
test('Engine identity defaults', python,
  args : [files('isolated.py'), engine_defaults_test], timeout : 15)

connection_ui_test = executable('test-connection-ui', 'test-connection-ui.c',
  '../../src/engine/elaro_command.c',
  '../../src/fe-efl/ui/elaro_ui_util.c',
  include_directories : rootinc,
  link_with : config_lib,
  dependencies : [elementary_dep, eet_dep, ecore_file_dep])
test('Connection save and connect actions', python,
  args : [files('isolated.py'), connection_ui_test], timeout : 15,
  env : ['ELM_ENGINE=buffer', 'ECORE_NO_SYSTEM_MODULES=1', 'EINA_LOG_LEVEL=0'])

avatar_ui_test = executable('test-avatar-ui', 'test-avatar-ui.c',
  '../../src/fe-efl/ui/elaro_ui_util.c',
  '../../src/engine/elaro_command.c',
  link_depends : elaro_assets,
  include_directories : rootinc,
  link_with : frontend_support,
  dependencies : [elementary_dep, ecore_file_dep])
test('Gravatar hover and cache lifecycle', python,
  args : [files('isolated.py'), avatar_ui_test], timeout : 15,
  env : ['ELM_ENGINE=buffer', 'ECORE_NO_SYSTEM_MODULES=1', 'EINA_LOG_LEVEL=0'])

reconnect_test = executable('test-reconnect', 'test-reconnect.c',
  include_directories : rootinc,
  link_with : [engine_lib, libconfig_a, libcore_a, libirc_a],
  dependencies : dep + [threads_dep],
  export_dynamic : true)
foreach scenario : ['success', 'limit', 'timeout', 'cancel', 'quit', 'disabled', 'quit-message', 'quit-default', 'quit-blank']
  test('IRC reconnect ' + scenario, python,
    args : [files('reconnect.py'), reconnect_test, scenario], timeout : 90)
endforeach

ctcp_test = executable('test-ctcp', 'test-ctcp.c',
  include_directories : rootinc,
  link_with : [engine_lib, libconfig_a, libcore_a, libirc_a],
  dependencies : dep + [threads_dep],
  export_dynamic : true)
test('CTCP reply formatting', ctcp_test)

bans_test = executable('test-bans', 'test-bans.c',
  include_directories : rootinc,
  link_with : [engine_lib, libconfig_a, libcore_a, libirc_a],
  dependencies : dep + [threads_dep, intl_dep],
  export_dynamic : true)
test('Channel ban management', python,
  args : [files('bans.py'), bans_test], timeout : 45)
bans_ui_test = executable('test-bans-ui', 'test-bans-ui.c',
  '../../src/engine/elaro_command.c',
  include_directories : rootinc,
  link_with : frontend_support,
  dependencies : [elementary_dep, ecore_file_dep])
test('Channel bans window', python,
  args : [files('isolated.py'), bans_ui_test], timeout : 15,
  env : ['ELM_ENGINE=buffer', 'ECORE_NO_SYSTEM_MODULES=1', 'EINA_LOG_LEVEL=0',
         'ELM_AUTO_NORENDER_WITHDRAWN=0'])

mode_test = executable('test-mode', 'test-mode.c',
  include_directories : rootinc,
  link_with : [engine_lib, libconfig_a, libcore_a, libirc_a],
  dependencies : dep + [threads_dep, intl_dep],
  export_dynamic : true)
test('IRC mode commands', python,
  args : [files('mode.py'), mode_test], timeout : 50)

list_test = executable('test-list', 'test-list.c',
  include_directories : rootinc,
  link_with : [engine_lib, libconfig_a, libcore_a, libirc_a],
  dependencies : dep + [threads_dep, intl_dep],
  export_dynamic : true)
test('IRC channel list', python,
  args : [files('list.py'), list_test], timeout : 50)
list_ui_test = executable('test-list-ui', 'test-list-ui.c',
  '../../src/engine/elaro_command.c',
  '../../src/fe-efl/ui/elaro_ui_text.c',
  include_directories : rootinc,
  link_with : frontend_support,
  dependencies : [elementary_dep, ecore_file_dep])
test('Channel list window', python,
  args : [files('isolated.py'), list_ui_test], timeout : 15,
  env : ['ELM_ENGINE=buffer', 'ECORE_NO_SYSTEM_MODULES=1', 'EINA_LOG_LEVEL=0',
         'ELM_AUTO_NORENDER_WITHDRAWN=0'])

presence_test = executable('test-presence', 'test-presence.c',
  include_directories : rootinc,
  link_with : [engine_lib, libconfig_a, libcore_a, libirc_a],
  dependencies : dep + [threads_dep],
  export_dynamic : true)
test('IRC away and back state', python,
  args : [files('presence.py'), presence_test], timeout : 50)

engine_wakeup_test = executable('test-engine-wakeup', 'test-engine-wakeup.c',
  include_directories : rootinc,
  link_with : [engine_lib, libconfig_a, libcore_a, libirc_a],
  dependencies : dep + [threads_dep],
  export_dynamic : true)
test('Engine queue wakeups and bursts', python,
  args : [files('isolated.py'), engine_wakeup_test], timeout : 15)

receive_test = executable('test-receive', 'test-receive.c',
  include_directories : rootinc,
  link_with : [engine_lib, libconfig_a, libcore_a, libirc_a],
  dependencies : dep + [threads_dep],
  export_dynamic : true)
openssl_program = find_program('openssl')
test('Two clients receive complete TLS bursts', python,
  args : [files('receive.py'), receive_test, openssl_program.full_path()], timeout : 30)

# Allocation recovery and cache insertion must also work when Eina's safety
# macros do not branch or evaluate their GOTO expressions.
allocation_failure_no_safety_test = executable('test-allocation-failure-no-safety',
  'test-allocation-failure.c',
  include_directories : rootinc,
  c_args : ['-DELARO_TEST_NO_SAFETY_CHECKS'],
  link_with : frontend_support,
  dependencies : [eet_dep, ecore_file_dep, intl_dep])
test('Configuration allocation failures without safety checks', allocation_failure_no_safety_test)

avatar_no_safety_test = executable('test-avatar-ui-no-safety', 'test-avatar-ui.c',
  '../../src/fe-efl/ui/elaro_ui_util.c',
  '../../src/engine/elaro_command.c',
  link_depends : elaro_assets,
  include_directories : rootinc,
  c_args : ['-DELARO_TEST_NO_SAFETY_CHECKS'],
  link_with : frontend_support,
  dependencies : [elementary_dep, ecore_file_dep])
test('Gravatar lifecycle without safety checks', python,
  args : [files('isolated.py'), avatar_no_safety_test], timeout : 15,
  env : ['ELM_ENGINE=buffer', 'ECORE_NO_SYSTEM_MODULES=1', 'EINA_LOG_LEVEL=0'])

cleanup_failure_test = executable('test-cleanup-failure', 'test-cleanup-failure.c',
  include_directories : rootinc,
  link_with : frontend_support,
  link_depends : elaro_assets,
  dependencies : [elementary_dep, ecore_file_dep])
test('Partial notification cleanup without safety checks', python,
  args : [files('isolated.py'), cleanup_failure_test], timeout : 15,
  env : ['ELM_ENGINE=buffer', 'ECORE_NO_SYSTEM_MODULES=1', 'EINA_LOG_LEVEL=0'])

themes_test = executable('test-themes', 'test-themes.c',
  '../../src/fe-efl/ui/elaro_ui_util.c', '../../src/fe-efl/ui/elaro_ui_about.c',
  '../../src/fe-efl/ui/elaro_ui_connection.c', '../../src/fe-efl/ui/elaro_ui_options.c',
  '../../src/engine/elaro_command.c',
  '../../src/fe-efl/ui/elaro_ui_history.c',
  include_directories : rootinc,
  c_args : ['-DELARO_BUILD_DIR="' + meson.project_build_root() + '"'],
  link_with : [frontend_support, config_lib],
  link_depends : elaro_assets,
  dependencies : [elementary_dep, ecore_file_dep])
test('Application themes and live switching', python,
  args : [files('isolated.py'), themes_test], timeout : 60,
  env : ['ELARO_TEST_TIMEOUT=55', 'ELM_ENGINE=buffer', 'ECORE_NO_SYSTEM_MODULES=1', 'EINA_LOG_LEVEL=0'])
