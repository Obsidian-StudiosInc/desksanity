project('desksanity', 'c',
	version: '1.1.99',
	license: 'BSD 2 clause',
	default_options: [ 'c_std=gnu99', 'warning_level=2' ],
	meson_version: '>= 0.40.0')

add_global_arguments('-DHAVE_CONFIG_H=1', language: 'c')
dir_prefix = get_option('prefix')
dir_bin = join_paths(dir_prefix, get_option('bindir'))
dir_sysconf = get_option('sysconfdir')
dir_data = join_paths(dir_prefix, get_option('datadir'))
dir_include = join_paths(dir_prefix, get_option('includedir'))
dir_include_e = join_paths(dir_include, meson.project_name())
dir_lib = join_paths(dir_prefix, get_option('libdir'))


pkgconfig = import('pkgconfig')
cc = meson.get_compiler('c')

# disable for release builds
dev_cflags = []
dev_cflags_try = [
	'-Wall',
	'-W',
	'-Wpointer-arith',
	'-Wshadow',
	'-Wno-missing-field-initializers',
	'-Wfloat-equal',
	'-Wuninitialized',
	'-Wundef',
	'-Wcast-align',
	'-Wformat=2',
	'-Wno-format-y2k',
]

foreach cf: dev_cflags_try
if cc.has_argument(cf) == true
dev_cflags += cf
endif
endforeach
add_global_arguments(dev_cflags, language: 'c')

release = 'ver-0.22'
host_os = host_machine.system()
if host_os == 'linux'
	if cc.has_header_symbol('features.h', '__UCLIBC__')
		host_os = 'linux-uclibc'
	elif cc.has_header_symbol('features.h', '__dietlibc__')
		host_os = 'linux-dietlibc'
	else
		host_os = 'linux-gnu'
	endif
endif

module_arch = '@0@-@1@-@2@'.format(host_os, host_machine.cpu_family(), release)

config_h = configuration_data()
config_h.set('_GNU_SOURCE', '1')
config_h.set('_ALL_SOURCE', '1')
config_h.set('_POSIX_PTHREAD_SEMANTICS', '1')
config_h.set('_TANDEM_SOURCE', '1')
config_h.set('__EXTENSIONS__', '1')

config_h.set_quoted('MODULE_ARCH', module_arch)
config_h.set_quoted('PACKAGE', meson.project_name())
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())
config_h.set_quoted('VERSION', meson.project_version())
config_h.set_quoted('PACKAGE_NAME', meson.project_name())
config_h.set_quoted('BINDIR', dir_bin)
config_h.set_quoted('DATADIR', dir_data)

if cc.has_function('setenv') == true
	config_h.set('HAVE_SETENV', '1')
endif
if cc.has_function('unsetenv') == true
	config_h.set('HAVE_UNSETENV', '1')
endif
if cc.has_function('clearenv') == true
	config_h.set('HAVE_CLEARENV', '1')
endif
if cc.has_header('features.h') == true
	config_h.set('HAVE_FEATURES_H', '1')
endif
if cc.has_header('sys/ptrace.h') == true
	config_h.set('HAVE_SYS_PTRACE_H', '1')
endif
if cc.has_header('arpa/inet.h') == true
	config_h.set('HAVE_ARPA_INET_H', '1')
endif
if cc.has_header('netinet/in.h') == true
	config_h.set('HAVE_NETINET_IN_H', '1')
endif
if cc.has_header('execinfo.h') == true
	config_h.set('HAVE_EXECINFO_H', '1')
elif cc.has_function('backtrace_symbols_fd', dependencies: 'execinfo') == false
	execinfo_dep = dependency('execinfo', required: false)
endif

if cc.has_header('fnmatch.h') == false
	error('fnmatch.h not found')
endif

if cc.has_function('fnmatch') == false
	dep_fnmatch = dependency('fnmatch', required: true)
endif

add_global_arguments('-DPACKAGE_BIN_DIR="@0@"'.format(dir_bin), language: 'c')
add_global_arguments('-DPACKAGE_LIB_DIR="@0@"'.format(dir_lib), language: 'c')
add_global_arguments('-DPACKAGE_DATA_DIR="@0@"'.format(join_paths(dir_data, meson.project_name())), language: 'c')
add_global_arguments('-DPACKAGE_SYSCONF_DIR="@0@"'.format(dir_sysconf), language: 'c')

dep_efl_wl = dependency('efl-wl', required: false)
dep_e = dependency('enlightenment')

dir_module_e = join_paths([dep_e.get_pkgconfig_variable('modules'), 'desksanity'])
edje_cc = find_program('edje_cc')

install_data(['e-module-desksanity.edj', 'module.desktop'],
	install_dir: dir_module_e,
	install_mode: 'rw-rw-r--')

build_files = [
	'src/e_mod_main.h',
	'src/e_mod_main.c',
	'src/ds_config.c',
	'src/maximize.c',
	'src/moveresize.c',
	'src/pip.c',
	'src/zoom.c',
	'src/magnify.c',
	'src/desksanity.c'
]

if dep_efl_wl.found() == true and dep_e.get_pkgconfig_variable('wayland') == 'true'
	build_files += 'src/runner.c'
	config_h.set('HAVE_RUNNER', '1')

	wayland_scanner = find_program('wayland-scanner')
	gen_scanner_client = generator(wayland_scanner,
			output: '@BASENAME@-client-protocol.h',
			arguments: ['client-header', '@INPUT@', '@OUTPUT@'])
	gen_scanner_server = generator(wayland_scanner,
			output: '@BASENAME@-server-protocol.h',
			arguments: ['server-header', '@INPUT@', '@OUTPUT@'])
	gen_scanner_impl = generator(wayland_scanner,
			output: '@BASENAME@-protocol.c',
			arguments: ['code', '@INPUT@', '@OUTPUT@'])
	protos = [
		'e-gadget.xml',
		'action_route.xml',
	]

	loader_src = ['loader/loader.c']
	foreach proto: protos
		loader_src += gen_scanner_client.process(proto)
		loader_src += gen_scanner_impl.process(proto)
	endforeach
	foreach proto: protos
		build_files += gen_scanner_server.process(proto)
		build_files += gen_scanner_impl.process(proto)
	endforeach

	shared_library('loader', loader_src,
		name_prefix: '',
		dependencies: [
			dependency('elementary'),
			dependency('ecore-wl2'),
			dependency('wayland-client'),
			cc.find_library('uuid')],
		install_dir: join_paths([dir_module_e, module_arch]),
		install: true)

	dir_gadgets = join_paths([dir_lib, 'enlightenment/gadgets', dep_e.get_pkgconfig_variable('release')])
	executable('e_gadget_start',
		'loader/start.c',
		c_args: '-fPIE',
		link_args: '-fPIE',
		dependencies: [dependency('elementary'), dependency('enlightenment')],
		install_dir: join_paths([dir_gadgets, 'start']),
		install: true)

	executable('e_gadget_test',
		'loader/test.c',
		c_args: '-fPIE',
		link_args: '-fPIE',
		dependencies: dependency('elementary'),
		install_dir: join_paths([dir_gadgets, 'test']),
		install: true)

	config_h.set_quoted('GADGET_DIR', dir_gadgets)
	desktop_data = configuration_data()
	desktop_data.set('GADGET_DIR', dir_gadgets)
	configure_file(input: 'loader/test.desktop.in',
		output: 'test.desktop',
		install: true,
		install_dir: join_paths([dir_gadgets, 'test']),
		configuration: desktop_data)
	configure_file(input: 'loader/start.desktop.in',
		output: 'start.desktop',
		install: true,
		install_dir: join_paths([dir_gadgets, 'start']),
		configuration: desktop_data)
endif


configure_file(output: 'config.h',
	install: false,
	configuration: config_h)

shared_module('desksanity', build_files,
	include_directories: include_directories(['src']),
	name_prefix: '',
	dependencies: [dep_e, dep_efl_wl],
	install_dir: join_paths([dir_module_e, module_arch]),
	install: true)


meson.add_install_script('meson_modules.sh', join_paths([dir_module_e, module_arch, 'desksanity.so']))
