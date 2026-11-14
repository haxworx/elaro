edje_cc = find_program('edje_cc')
elaro_assets = custom_target('elaro.edj',
  input : 'elaro.edc',
  output : 'elaro.edj',
  depend_files : files('images/menu.png', 'images/person.svg', 'elaro.png'),
  command : [edje_cc, '-id', meson.current_source_dir() / 'images',
             '-id', meson.current_source_dir(), '@INPUT@', '@OUTPUT@'],
  build_by_default : true,
  install : true,
  install_dir : get_option('datadir') / 'elaro')
