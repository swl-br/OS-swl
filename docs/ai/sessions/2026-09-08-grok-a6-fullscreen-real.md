# Sessão — 2026-09-08

IA: Grok
Data: 2026-09-08
Responsável: A6 — fullscreen real (não só schedule_configure)

## Escopo

Apenas `swl-ui/src/swlwm.c`.

Base: `origin/main` (inclui A1/A2/R-06). Inclui também o reflow de
maximizadas no resize de output (A5), porque o mesmo caminho serve
fullscreen — se A5 ainda não estiver mergeada, este patch cobre os dois.

## Problema

`xdg_toplevel_request_fullscreen` só chamava
`wlr_xdg_surface_schedule_configure` — o protocolo respondia, mas a
janela não mudava de tamanho nem escondia a titlebar.

## Solução

- Flag `bool fullscreen` no toplevel.
- `toplevel_apply_fullscreen_layout`: posição (0,0), conteúdo sem
  offset de titlebar, decoração desabilitada, `set_size(screen)`,
  `set_fullscreen(true)`.
- `toplevel_set_fullscreen(fs)`:
  - entrar: salva geo se não maximizado; se maximizado, mantém flag e
    `saved_geo`; aplica layout; raise acima do shell.
  - sair: se maximizado → layout maximizado; senão → restore geo;
    reabilita titlebar; raise painel/taskbar de novo.
- `xdg_toplevel_request_fullscreen` chama `toplevel_set_fullscreen`
  com `requested.fullscreen`.
- Maximize e fullscreen são mutuamente exclusivos na geometria ativa
  (maximize cancela fullscreen).
- Resize de output: reflow de fullscreen e maximizadas.
- Clique em decoração / resize por borda ignoram fullscreen.

## Arquivo

- `swl-ui/src/swlwm.c`

## Teste sugerido

```bash
cd swl-ui && ninja -C build
WLR_BACKEND=x11 ./build/swlwm -s "foot"
# no foot: fullscreen (F11 se suportado) ou pedir via protocolo
# janela deve cobrir a tela sem titlebar
# sair do fullscreen restaura tamanho; se estava maximizado, volta maximizado
# redimensionar o output com fullscreen ativo → acompanha
```

## Nota A5

Este patch também implementa A5 (`toplevel_apply_maximized_layout` +
reflow em `output_request_state`). Se a entrega A5 separada já tiver
sido mergeada, o orquestrador deve conferir conflito — a lógica é a
mesma.
