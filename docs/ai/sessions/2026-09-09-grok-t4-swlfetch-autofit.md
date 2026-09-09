# Sessão — 2026-09-09

IA: Grok
Data: 2026-09-09
Responsável: T4 fatia — swlfetch/Neo auto-ajusta à largura

## Escopo

Apenas `userland/swlfetch` (v5).

## Comportamento

| Largura terminal | Layout |
|------------------|--------|
| ≥ info+gap+40 (~71) | lado a lado (como antes) |
| entre 48 e 70 | lado a lado com coluna de info encolhida |
| < 48 | info em cima, arte embaixo |

Detecta `COLUMNS` ou `stty size`. `SWL_ART_W` sobrescreve a
largura da arte (padrão 40, desenho atual do neo-face).

## Teste

```bash
COLUMNS=100 sh userland/swlfetch   # side
COLUMNS=55  sh userland/swlfetch   # side estreito
COLUMNS=35  sh userland/swlfetch   # stack
```
