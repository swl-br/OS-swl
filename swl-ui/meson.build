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
# Xubuntu). Não declarar wayland-protocols — não é usado no build
# (wlroots já puxa o que precisa).
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

# R-10: xdg-shell-protocol.c/.h pré-gerados (v7, ~72KB) removidos —
# o compositor usa só a API do wlroots (wlr_xdg_shell_create versão 3).

executable('swlwm',
  [
    'src/swlwm.c',
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
