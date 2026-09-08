project('tswl', 'c',
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
# R-09: sem -lm / -lrt — nenhum <math.h>; clock_gettime está na libc
# moderna (não precisa librt).

# xdg-shell: gerado em modo CLIENTE a partir do XML do wayland-protocols
# (o arquivo do swl-ui era modo servidor — não serve pra apps).
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

executable('tswl',
  [
    'src/main.c',
    'src/term.c',
    'src/pty.c',
    'src/render.c',
    xdg_shell_code,
    xdg_shell_client_h,
  ],
  include_directories: include_directories('include'),
  c_args: ['-D_GNU_SOURCE'],
  dependencies: [wayland_client, xkbcommon, cairo, pangocairo],
  install: true,
)
