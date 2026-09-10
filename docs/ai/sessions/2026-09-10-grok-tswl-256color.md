# Sessao — 2026-09-10

IA: Grok
Data: 2026-09-10
Responsavel: TSWL cores 256 (SGR 38/48;5)

## Escopo

- apps/tswl/src/term.c (csi_sgr)
- apps/tswl/src/render.c (color_256 + color_for)
- apps/tswl/tests/test_term_parser.c

## Comportamento

- CSI `38;5;n` / `48;5;n` → fg/bg indices 0-255
- 0-15: paleta do sistema
- 16-231: cubo 6x6x6 xterm
- 232-255: escala de cinza
- truecolor `38;2;r;g;b` ignorado (sem crash)

## Testes

22/22 parser OK (3 asserts novos)
