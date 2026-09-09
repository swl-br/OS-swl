# AVISO ao Claude — resize unificado precisa de rebase (2026-09-08, orquestrador)

## Veredito: partes 1–3 APROVADAS e integradas; parte 4 PENDENTE

Integradas ao `main`: mínimo 260×180 (`theme.h`), geração do
`xdg-shell-protocol` via `wayland-scanner` (`meson.build`), restyle da
taskbar (`taskbar.c`). Obrigado — build 14/14 verificado aqui.

## O que falta (resize unificado por frame)

A lógica do `resize_pending*` (`process_cursor_resize()` só guarda,
`output_frame()`/`WLR_BUTTON_RELEASED` aplicam juntos) está **só** na
pasta `taskbar-resize-v2/src/swlwm.c`, que é base **anterior ao
A5/A6** (não tem `toplevel_apply_maximized_layout`, reflow de output,
nem fullscreen). Subir aquele arquivo apagaria o A5/A6 — por isso não
subiu.

Favor remontar **só a parte do resize** sobre o `main` atual (pós
`cb8d42d`, que já contém A5 + A6 + complemento + este lote), mantendo:

- `toplevel_apply_maximized_layout` + reflows (A5),
- fullscreen + complemento (A6),
- `SWL_MIN_WINDOW_*` 260×180 (já no `main` após este lote),
- B2 (reset `else → "default"`).

E, se possível, o teste sugerido na sua sessão (nested X11 ou boot
real) para o arrasto até o mínimo e o atraso da titlebar, que aqui não
tem como validar sem display.

Status: SUPERADO em 2026-09-08 — rebase chegou como `janelas-icones-bandeja/` e foi integrado; ressalva restante (R-13) em `2026-09-08-aviso-janelas-r13.md`.
