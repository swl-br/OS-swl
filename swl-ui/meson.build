project('swl-ui', 'c',
  version: '0.1.0',
  default_options: [
    'c_std=c11',
    'warning_level=2',
    'buildtype=release',
  ],
)

# R-10 / DEC-006: preferir 0.18 (validado) antes de 0.19 (nunca validado
# com o guard SWL_WLR_0_18). Depois cai no wlroots genérico (0.17 no
# Xubuntu).
wlroots = dependency('wlroots-0.18', required: false)
if not wlroots.found()
  wlroots = dependency('wlroots', required: false)
endif
if not wlroots.found()
  wlroots = dependency('wlroots-0.19', required: false)
endif
if not wlroots.found()
  error('wlroots não encontrado (tentou 0.18, genérico e 0.19)')
endif

wayland_server = dependency('wayland-server')
xkbcommon  = dependency('xkbcommon')
cairo      = dependency('cairo')
pangocairo = dependency('pangocairo')
libdrm     = dependency('libdrm')
mathlib    = meson.get_compiler('c').find_library('m', required: false)

# R-10 tinha removido xdg-shell-protocol.c/.h pré-gerados (v7, ~72KB)
# achando que era código morto — mas `wlr/types/wlr_xdg_shell.h` (header
# do PRÓPRIO wlroots, não nosso) faz `#include "xdg-shell-protocol.h"`
# internamente, então o arquivo precisa existir no include path mesmo
# sem nosso código chamar nada dele diretamente. A correção de verdade
# não é reverter a remoção (voltar a commitar um binário gerado de
# ~72KB) — é gerar de verdade em tempo de build via wayland-scanner,
# como todo compositor wlroots faz (é o padrão do próprio tinywl, que
# esse projeto usa como base).
wayland_scanner = find_program('wayland-scanner')
wayland_protocols = dependency('wayland-protocols')
wl_protocol_dir = wayland_protocols.get_variable('pkgdatadir')
xdg_shell_xml = wl_protocol_dir / 'stable/xdg-shell/xdg-shell.xml'

xdg_shell_protocol_h = custom_target('xdg-shell-protocol.h',
  input: xdg_shell_xml,
  output: 'xdg-shell-protocol.h',
  command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@'],
)
xdg_shell_protocol_c = custom_target('xdg-shell-protocol.c',
  input: xdg_shell_xml,
  output: 'xdg-shell-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
)

executable('swlwm',
  [
    'src/swlwm.c',
    xdg_shell_protocol_c,
    xdg_shell_protocol_h,
    'src/swl_buffer.c',
    'src/swl_draw_util.c',
    'src/panel.c',
    'src/taskbar.c',
    'src/decorations.c',
    'src/desktop.c',
    'src/background.c',
    'src/menu.c',
    'src/context_menu.c',
  ],
  include_directories: include_directories('include'),
  # -DWLR_USE_UNSTABLE: exigido pelos headers do wlroots.
  # -D_DEFAULT_SOURCE: c_std=c11 puro esconde M_PI e strdup na glibc.
  c_args: ['-DWLR_USE_UNSTABLE', '-D_DEFAULT_SOURCE'],
  dependencies: [
    wlroots, wayland_server, xkbcommon, cairo, pangocairo, libdrm, mathlib,
  ],
  install: true,
)
