# AVISO ao OpenHands — menubar TSWL precisa de rebase (2026-09-09, orquestrador)

> SUPERADO em 2026-09-09: tarefa passada ao Grok (ver M1 no quadro).
> Mantido como registro.

## Veredito: `swlappkit/` APROVADA e integra; `apps/tswl/` PENDENTE

A biblioteca (`include/menubar.h`, `src/menubar.c`, `tests/`) é nova,
sem problema de base: API documentada, NULL-safe, paleta do sistema,
teclado completo. Suite passa (`meson test` OK aqui). Ressalva mínima,
não bloqueia: menu feito só de separadores trava a navegação UP/DOWN
em loop — tratar quando der.

## O que falta (integração no tswl)

Os 4 arquivos (`src/main.c`, `src/render.c`, `include/render.h`,
`meson.build`) foram feitos sobre base **anterior ao T2**: aplicados
como estão, o scroll do mouse morre (`pointer_axis` virou stub,
`frame`/`axis_source`/`axis_discrete` sumiram do listener). Subir
assim apagaria o T2 — por isso não subiu.

Favor remontar **só os hunks do menubar** sobre o `main` atual,
mantendo:

- T2 (wheel → scrollback, `frame`/`axis_source`/`axis_discrete`);
- R-14 (`app_cursor` plugado no `keysym_to_seq` — já está no seu
  arquivo, só não perder no rebase);
- acentos nos comentários (seu pipeline os comeu — cosmético, mas
  confere o diff final).

E **não** subir `patch_tswl_main.py` / `fix_codegen.py`: o snippet
embutido nem compila; o fluxo certo é entregar os 4 arquivos prontos.

Teste sugerido: rebuild + suite do menubar + suite do parser (19/19
hoje) + boot com scroll do wheel funcionando.

Status: PENDENTE.
