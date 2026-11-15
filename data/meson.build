edje_cc = find_program('edje_cc')
elaro_assets = [custom_target('elaro.edj',
  input : 'elaro.edc',
  output : 'elaro.edj',
  depend_files : files('images/menu.png', 'images/person.svg', 'elaro.png', 'sounds/icq.wav'),
  command : [edje_cc, '-sd', meson.current_source_dir() / 'sounds', '-id', meson.current_source_dir() / 'images',
             '-id', meson.current_source_dir(), '@INPUT@', '@OUTPUT@'],
  build_by_default : true,
  install : true,
  install_dir : get_option('datadir') / 'elaro')]

# Four independently compiled and installed application themes.
foreach name : ['midnight', 'porcelain', 'verdigris', 'ember']
  theme_icons = []
  foreach icon : ['menu', 'application', 'settings', 'close', 'minimize', 'maximize',
                  'theme', 'quit', 'chat', 'user-available', 'user-idle', 'info',
                  'emblem-important', 'audio-input-microphone', 'about',
                  'button', 'button-hover', 'button-pressed', 'handle', 'handle-h']
    theme_icons += files('themes' / name / (icon + '.svg'))
  endforeach
  elaro_assets += custom_target(name + '.edj',
    input : 'themes' / name / (name + '.edc'),
    output : name + '.edj',
    depend_files : theme_icons + files('themes/widgets.edc', 'themes/progress.edc'),
    command : [edje_cc, '-id', meson.current_source_dir() / 'themes' / name, '@INPUT@', '@OUTPUT@'],
    build_by_default : true,
    install : true,
    install_dir : get_option('datadir') / 'elaro')
endforeach
