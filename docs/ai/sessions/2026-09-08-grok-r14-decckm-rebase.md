# Sessão — 2026-09-08

IA: Grok
Data: 2026-09-08
Responsável: R-14 rebase — DECCKM + F1–F12 **sobre term.c com R-11**

## Contexto

Aviso `docs/revisao/2026-09-08-aviso-grok-r14.md`: entrega anterior
aprovada em lógica, mas `term.c` sem R-11 — subir apagaria R-11.

## Base

`origin/main` `19ec5d4` (R-11 já integrado em `6a6e424`).

## Mudanças (aditivas)

### `term.c` (3 acréscimos, R-11 intacto)
1. `bool app_cursor` no struct
2. Getter `tswl_term_app_cursor()`
3. Ramo CSI private `?1` h/l (DECCKM) no dispatch existente

### `term.h`
- Declaração de `tswl_term_app_cursor`

### `main.c`
- Tabela F1–F12
- `keysym_to_seq(..., tswl_term_app_cursor(a->term))`

## Verificação estática

- `R-11` + `0x9B` ainda presentes em `term.c`
- DECCKM + getter + F-keys + wire presentes

## Arquivos

- `apps/tswl/src/term.c`
- `apps/tswl/src/main.c`
- `apps/tswl/include/term.h`
