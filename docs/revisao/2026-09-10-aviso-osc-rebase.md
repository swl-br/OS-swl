# AVISO — OSC title: rebase sobre term.c com 256 (2026-09-10, orquestrador)

## Veredito: LÓGICA APROVADA (27/27), integração BLOQUEADA por base velha

O `term.c` entregue não tem os blocos 256-color (SGR 38;5/48;5) —
subir apagaria o 256 integrado ontem. Lógica OSC verificada e correta.

## O que fazer (só adições, sobre o term.c atual com 256)

1. `#include <stdio.h>` (pro `snprintf`).
2. Campos `osc_buf[256]`, `osc_len`, `window_title[256]`,
   `title_pending` no struct + init.
3. Captura em ST_OSC_STR (BEL/ST, `0;`/`2;`, trunc 255, `b >= 0x20`) +
   `osc_len = 0` na entrada `]`.
4. `tswl_term_take_title()` no fim (com guards).
5. `term.h`: decl. `main.c` e testes: como entregues (base compatível).

Status: PENDENTE.
