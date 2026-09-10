# Sessão — 2026-09-09

IA: Grok
Data: 2026-09-09
Responsável: T3 — rebase mínimo da linha de fonte (aviso)

## Contexto

`docs/revisao/2026-09-09-aviso-t3-fontline.md`: base fonts.conf já no
main; faltava só a lista de famílias no `render.c` sem apagar T1.

## Mudança

Única alteração em `apps/tswl/src/render.c`:

```c
#define TSWL_FONT "JetBrains Mono, Fira Code, DejaVu Sans Mono, DejaVu Sans, monospace"
```

Cursor barra (T1) permanece intacto.
