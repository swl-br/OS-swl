# Sessão — 2026-09-08

IA: Grok
Data: 2026-09-08
Responsável: unit tests do parser ANSI do TSWL (infra de regressão)

## Escopo

Só `apps/tswl/tests/` (novo). Não mexe em `term.c` / GUI / swlc.

## Motivação

AFAZERES / IDEIAS: "ainda não existe infra de teste no repo; primeiro
passo razoável é unit test do parser ANSI".

## Entrega

- `apps/tswl/tests/test_term_parser.c` — 13 asserts:
  - texto + CRLF, SGR bold, CUP clamp
  - R-01 CSI r inválido + scroll
  - R-03 param gigante
  - R-11 ESC cancela CSI, C1 sem glyph, CSI 8-bit `0x9B`
  - R-14 DECCKM `?1h`/`?1l`
- `apps/tswl/tests/run_parser_tests.sh` — compila só `term.c` (sem
  Wayland/Cairo); `TSWL_SANITIZE=1` liga ASan/UBSan

## Verificação

```
$ sh apps/tswl/tests/run_parser_tests.sh
summary: 13 passed, 0 failed
```
