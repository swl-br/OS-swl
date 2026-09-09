# Sessão — 2026-09-09

IA: Grok
Data: 2026-09-09
Responsável: T2 — scroll por wheel + teclas (fatia de T4)

## Escopo

Apenas `apps/tswl/src/main.c`.

## Problema

- Wheel do mouse não existia (só teclado).
- Shift+PageUp no QEMU às vezes não chega como `Page_Up`.

## Solução

1. `wl_pointer` no seat: `axis` vertical → `tswl_term_scroll_view` (±3 linhas).
2. Teclas de scrollback:
   - Shift+PageUp/Down e **KP_PageUp/Down**
   - Shift+Up/Down (e KP) em passos de 3 linhas
3. `axis_discrete` no-op (evita scroll duplicado com `axis`).

## Teste sugerido (QEMU)

- Gerar histórico (`seq 1 200`) e:
  - roda do mouse
  - Shift+PageUp / Shift+↑
