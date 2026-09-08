# Sessão — 2026-09-08

IA: Grok
Data: 2026-09-08
Responsável: A7 — correções de build/leveza (R-09, R-10)

## Escopo

- `apps/tswl/meson.build` (R-09)
- `swl-ui/meson.build` (R-10)
- Remoção de `swl-ui/src/xdg-shell-protocol.c` e
  `swl-ui/include/xdg-shell-protocol.h` (mortos)

Não mexeu em: R-11 (parser ANSI — outro bug, não é “build/leveza”),
código C de apps/compositor além dos meson, A6.

Base: `origin/main` (`f1c4662`, A5+B3 mergeados).

## Mudanças

### R-09 — TSWL sem `-lm` / `-lrt`
- Removidos `mathlib` e `rt` do `meson.build`.
- Nenhum `<math.h>` no tswl; `clock_gettime` está na libc moderna.
- `wayland-protocols` **mantido**: é usado de verdade pelo
  `wayland-scanner` pra gerar o xdg-shell **cliente**.

### R-10 — swl-ui
- Ordem de fallback wlroots: **0.18 → genérico → 0.19** (antes 0.19
  primeiro, combinação nunca validada com `SWL_WLR_0_18` / DEC-006).
- Removida dependência `wayland-protocols` (não era usada no meson).
- Removidos `src/xdg-shell-protocol.c` (~6KB gerado, v7) e o header
  correspondente — nada no compositor incluía esses arquivos; a API
  vem do `wlr_xdg_shell` (versão 3).
- `mathlib` **mantido**: `fabs`/`sin`/`cos`/`M_PI` em `swlwm.c`,
  `desktop.c`, `background.c`.

## Verificação sugerida

```bash
cd apps/tswl && rm -rf build && meson setup build && ninja -C build
# ldd build/tswl  → sem libm/librt extras se não vierem de cairo

cd swl-ui && rm -rf build && meson setup build && ninja -C build
# deve achar wlroots 0.18 se instalado; build 12/12 como antes
```

## Arquivos

- `apps/tswl/meson.build`
- `swl-ui/meson.build`
- `swl-ui/src/xdg-shell-protocol.c` (removido)
- `swl-ui/include/xdg-shell-protocol.h` (removido)
