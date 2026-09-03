project('swl-ui', 'c',
  version: '0.1.0',
  default_options: [
    'c_std=c11',
    'warning_level=2',
    'buildtype=release',
  ],
)

wlroots    = dependency('wlroots-0.19', required: false)
if not wlroots.found()
  wlroots  = dependency('wlroots', required: true)
endif
wayland_server = dependency('wayland-server')
wayland_protos = dependency('wayland-protocols')
xkbcommon  = dependency('xkbcommon')
cairo      = dependency('cairo')
pangocairo = dependency('pangocairo')
libdrm     = dependency('libdrm')
mathlib    = meson.get_compiler('c').find_library('m', required: false)

# Observação: xdg-shell-protocol.h/.c aqui são os mesmos arquivos já
# gerados que estavam em compositor/swlwm/ (copiados como estão). Se vocês
# preferirem gerar via wayland-scanner no build, é só trocar por um
# custom_target() apontando pro XML do wayland-protocols.

executable('swlwm',
  [
    'src/swlwm.c',
    'src/xdg-shell-protocol.c',
    'src/swl_buffer.c',
    'src/swl_draw_util.c',
    'src/panel.c',
    'src/taskbar.c',
    'src/decorations.c',
    'src/desktop.c',
    'src/background.c',
  ],
  include_directories: include_directories('include'),
  # -DWLR_USE_UNSTABLE: exigido pelos headers do wlroots (a API ainda não é
  # considerada estável pelo projeto upstream, mas é a única que existe).
  # -D_DEFAULT_SOURCE: com c_std=c11 puro (sem extensões gnu), a glibc
  # esconde M_PI (math.h) e strdup (string.h) atrás das feature test
  # macros; isso reabilita as duas sem precisar trocar pra 'gnu11'.
  c_args: ['-DWLR_USE_UNSTABLE', '-D_DEFAULT_SOURCE'],
  dependencies: [
    wlroots, wayland_server, xkbcommon, cairo, pangocairo, libdrm, mathlib,
  ],
  install: true,
)
