# AVISO — bell: falta o term.c (2026-09-11, orquestrador)

## Veredito: IDEIA APROVADA, entrega INCOMPLETA

Flash branco 120ms no BEL está certo. Mas o pacote só traz `main.c`
(chama `tswl_term_take_bell()`) + decl — sem o `term.c` que implementa.
Linkaria quebrado (`undefined reference`).

## O que falta

Em `apps/tswl/src/term.c`, sobre a base atual:

1. Flag `bell_pending` no struct + init.
2. Em ST_GROUND, byte `0x07` (fora de OSC): setar flag.
3. `bool tswl_term_take_bell(tswl_term *t)` (consome a flag, com guards
   NULL — mesmo padrão do `take_title`).

Status: ATENDIDO em 2026-09-11 — pacote completo (base atual, sem regressão) verificado e integrado.
