# Sessão — 2026-09-08

IA: Grok
Data: 2026-09-08
Responsável: TSWL — scrollback não zerar no resize

## Escopo

- `apps/tswl/src/term.c` (`tswl_term_resize`)
- `apps/tswl/tests/test_term_parser.c` (2 asserts novos)

Base: `origin/main` (com R-14 e unit tests).

## Problema

No resize, o ring de histórico era descartado (`back_count = 0`).
Shift+PageUp perdia o contexto após redimensionar a janela.

## Solução

Migrar cada linha do ring antigo → novo (ajustando colunas com
truncate/pad via células blank). Preserva `back_count` e clampa
`scroll_offset`.

## Testes

```
sh apps/tswl/tests/run_parser_tests.sh
# summary: 15 passed, 0 failed
```
