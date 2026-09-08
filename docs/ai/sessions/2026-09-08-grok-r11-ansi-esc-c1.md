# Sessão — 2026-09-08

IA: Grok
Data: 2026-09-08
Responsável: R-11 — ESC cancela CSI; C1 não viram U+FFFD

## Escopo

Apenas `apps/tswl/src/term.c`.

Base: `origin/main` (`cec64cb`).

## Problema

1. `ESC[1;2 <ESC> A` era engolido dentro de CSI (params corrompidos /
   sequência incompleta). Terminais reais cancelam CSI e iniciam novo ESC.
2. Bytes C1 (0x80-0x9F) caíam no caminho UTF-8 inválido → U+FFFD visível.

## Solução

- Em `ST_CSI`, byte `0x1B` → `ST_ESC`, zera params/private.
- Em `ST_GROUND`, `0x80-0x9F`: ignorados; `0x9B` inicia CSI (forma 8-bit).

## Arquivo

- `apps/tswl/src/term.c`

## Teste sugerido

Harness adversarial (já usado pelo orquestrador) + casos:

```
ESC [ 1 ; 2 ESC [ A     # cursor sobe 1; CSI anterior cancelado
0x9B 'A'                # CSI 8-bit = cursor up 1
0x80 0x9F               # não devem aparecer glyphs
```
