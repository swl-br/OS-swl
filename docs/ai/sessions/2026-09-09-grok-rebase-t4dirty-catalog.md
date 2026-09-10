# Sessão — 2026-09-09

IA: Grok
Data: 2026-09-09
Responsável: rebase avisos t4-dirty + tswl-catalog

## Avisos

- `docs/revisao/2026-09-09-aviso-t4-dirty.md`
- `docs/revisao/2026-09-09-aviso-tswl-catalog.md`

Base: `origin/main` (`3797f3c`).

## t4-dirty

Só a otimização dirty-row em `apps/tswl/src/render.c`:
- redraw completo se todas as linhas dirty
- senão só dirty + linha do cursor (e anterior)
- preserva: T3 (`DejaVu`), menubar (`swl_menubar_draw` + translate),
  cursor do `main` atual

## tswl-catalog

1. `desktop.c`: `/bin/tswl`, `/bin/swlpad` (find_free_slot intacto)
2. `swlwm.c`: `swl_launch_command()` + 3 call sites (R-13 e
   resize_pending intactos)
3. `build-gui-rootfs.sh`: aviso se faltar tswl; PATH/XDG no
   start-gui.sh; fonts.conf T3 intacto
