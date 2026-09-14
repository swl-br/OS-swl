project('swlcentral', 'c',
  version: '2.0.0',
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

executable('swlcentral',
  [
    'src/main.c',
    'src/ui.c',
    'src/pages.c',
    'src/api.c',
    'src/data.c',
    '../swlappkit/src/menubar.c',
    '../swlappkit/src/swl_shapes.c',
    '../swlappkit/src/swl_controls.c',
    '../swlappkit/src/swl_overlays.c',
    '../swlappkit/src/swl_misc.c',
    xdg_shell_code,
    xdg_shell_client_h,
  ],
  include_directories: include_directories('include', '../swlappkit/include'),
  c_args: ['-D_GNU_SOURCE'],
  dependencies: [wayland_client, xkbcommon, cairo, pangocairo],
  install: true,
)
