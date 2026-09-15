project('swlconfig', 'c',
  version: '0.1.0',
  default_options: [
    'c_std=c11',
    'warning_level=2',
    'buildtype=release',
  ],
)

wayland_client = dependency('wayland-client')
wayland_protos = dependency('wayland-protocols')
xkbcommon      = dependency('xkbcommon')
cairo          = dependency('cairo')
pangocairo     = dependency('pangocairo')
glib_dep       = meson.get_compiler('c').find_library('glib-2.0', required: false)
if not glib_dep.found()
  glib_dep = dependency('glib-2.0')
endif

# xdg-shell: gerado em modo CLIENTE a partir do XML do wayland-protocols
# (mesmo padrão do apps/tswl/meson.build).
wayland_scanner = find_program('wayland-scanner')
wl_protocols_dir = wayland_protos.get_variable('pkgdatadir')
xdg_shell_xml = join_paths(wl_protocols_dir, 'stable/xdg-shell/xdg-shell.xml')

xdg_shell_client_h = custom_target('xdg-shell-client-header',
  output: 'xdg-shell-client-protocol.h',
  input: xdg_shell_xml,
  command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
)
xdg_shell_code = custom_target('xdg-shell-code',
  output: 'xdg-shell-protocol.c',
  input: xdg_shell_xml,
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
)

swlconfig_inc = include_directories('include', '../swlappkit/include')

executable('swlconfig',
  [
    'src/main.c',
    'src/sysinfo.c',
    '../swlappkit/src/menubar.c',
    xdg_shell_code,
    xdg_shell_client_h,
  ],
  include_directories: swlconfig_inc,
  c_args: ['-D_GNU_SOURCE'],
  dependencies: [wayland_client, xkbcommon, cairo, pangocairo, glib_dep],
  install: true,
)

# sysinfo.c é testável isoladamente (parsers puros de meminfo/uptime),
# mesmo princípio do editor.c do SWLPad — ver README seção 17.
test_sysinfo = executable('test_sysinfo',
  [
    'tests/test_sysinfo.c',
    'src/sysinfo.c',
  ],
  include_directories: swlconfig_inc,
)
test('sysinfo parsers', test_sysinfo)
