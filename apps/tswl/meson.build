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
mathlib        = meson.get_compiler('c').find_library('m', required: false)
rt             = meson.get_compiler('c').find_library('rt', required: false)

# xdg-shell: gerado em modo CLIENTE a partir do XML do wayland-protocols
# (o arquivo do swl-ui é modo servidor — não serve pra apps).
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
  dependencies: [wayland_client, xkbcommon, cairo, pangocairo, mathlib, rt],
  install: true,
)
