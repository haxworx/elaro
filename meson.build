project('elaro', 'c',
  version : '0.1',
  meson_version : '>=0.58',
  default_options : ['warning_level=1', 'c_std=gnu99'])

irssi_base_version = '1.4.5'
cc = meson.get_compiler('c')
rootinc = include_directories('.')
chat_modules = ['irc']

dep = [
  dependency('glib-2.0', version : '>=2.52'),
  dependency('gmodule-2.0'),
  dependency('openssl'),
]
foreach name : ['nsl', 'socket', 'network']
  library = cc.find_library(name, required : false)
  if library.found()
    dep += library
  endif
endforeach

want_gregex = true
have_capsicum = false
have_libutf8proc = false
utf8proc = cc.find_library('utf8proc', required : false)
if utf8proc.found() and cc.has_function('utf8proc_version', dependencies : utf8proc)
  dep += utf8proc
  have_libutf8proc = true
endif

def_moduledir = '-DMODULEDIR="' + (get_option('prefix') / get_option('libdir') / 'elaro/modules') + '"'
def_sysconfdir = '-DSYSCONFDIR="' + (get_option('prefix') / get_option('sysconfdir')) + '"'

# Like Evisum, use libintl where needed and libc gettext otherwise.
intl_dep = cc.find_library('intl', required : false)
gettext_code = '''
#include <libintl.h>
int main(void) {
  bindtextdomain("elaro", "/tmp");
  bind_textdomain_codeset("elaro", "UTF-8");
  textdomain("elaro");
  return ngettext("message", "messages", 2) == 0;
}
'''
if not intl_dep.found()
  intl_dep = declare_dependency()
endif
have_gettext = cc.links(gettext_code, dependencies : intl_dep, name : 'gettext support')

conf = configuration_data()
conf.set10('ENABLE_NLS', have_gettext)
conf.set_quoted('LOCALEDIR', get_option('prefix') / get_option('localedir'))
conf.set('HAVE_GMODULE', true)
conf.set('USE_GREGEX', true)
conf.set('HAVE_LIBUTF8PROC', have_libutf8proc)
conf.set10('_DARWIN_USE_64_BIT_INODE', true)
conf.set_quoted('FHS_PREFIX', '')
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('ELARO_DATA_DIR', get_option('prefix') / get_option('datadir') / 'elaro')
conf.set_quoted('ELARO_ASSET_BUILD_DIR', meson.project_build_root() / 'data')
conf.set_quoted('ELARO_SOURCE_DIR', meson.project_source_root())
conf.set_quoted('PACKAGE_TARNAME', meson.project_name())
foreach header : ['sys/ioctl.h', 'sys/resource.h', 'sys/time.h', 'sys/utsname.h', 'dirent.h', 'unistd.h']
  conf.set('HAVE_' + header.underscorify().to_upper(), cc.has_header(header))
endforeach
# Generate configuration and version headers for the application and embedded core.
configure_file(output : 'elaro-config.h', configuration : conf)

version_conf = configuration_data()
version_conf.set('ELARO_VERSION_DATE', 20261112)
version_conf.set('ELARO_VERSION_TIME', 1405)
elaro_version_h = configure_file(output : 'elaro-version.h', configuration : version_conf)

python = find_program('python3')

add_project_arguments(cc.get_supported_arguments('-fno-strict-aliasing'), language : 'c')
if have_gettext
  subdir('po')
endif
subdir('data')
subdir('src')
subdir('tests')

install_data('data/elaro.desktop',
  install_dir : get_option('datadir') / 'applications')

install_data('data/elaro.png',
  install_dir : get_option('datadir') / 'icons' / 'hicolor' / '512x512' / 'apps')
install_data('data/images/elaro.svg',
  install_dir : get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps')
install_data('data/images/elaro-128.png', rename : 'elaro.png',
  install_dir : get_option('datadir') / 'icons' / 'hicolor' / '128x128' / 'apps')

about_conf = configuration_data()
about_conf.set('VERSION', meson.project_version())
about_conf.set('IRSSI_VERSION', irssi_base_version)
configure_file(input : 'data/ABOUT.in', output : 'ABOUT',
  configuration : about_conf,
  install_dir : get_option('datadir') / 'elaro')
install_data('AUTHORS', 'COPYING', install_dir : get_option('datadir') / 'elaro')
