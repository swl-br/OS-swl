# Sessão — 2026-09-08

IA: Grok
Data: 2026-09-08
Responsável: Complemento A6 — aviso `docs/revisao/2026-09-08-aviso-grok-a6.md`

## Escopo

Apenas `swl-ui/src/swlwm.c`, sobre o `main` atual (`cec64cb`).

## Item 1 (obrigatório)

No `server_new_output`, ramo `else` (shell já existe), após
`swl_menu_resize`, reflow estendido:

- pula `minimized`
- `fullscreen` → `toplevel_apply_fullscreen_layout`
- `maximized` → `toplevel_apply_maximized_layout`

Mesmo padrão já presente em `output_request_state`.

## Item 2 (opcional, feito)

Quando `toplevel_set_maximized(true)` cancela fullscreen ativo, re-raise
de painel e taskbar (igual à saída normal de fullscreen).

## Arquivo

- `swl-ui/src/swlwm.c`

## Resultado esperado

A6 deixa de ser PARCIAL no AFAZERES após verificação do orquestrador.
