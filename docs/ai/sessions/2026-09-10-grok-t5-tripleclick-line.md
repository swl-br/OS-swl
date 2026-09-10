# Sessao — 2026-09-10

IA: Grok
Data: 2026-09-10
Responsavel: T5+ triplo clique seleciona linha

Base: origin/main 90177e1

## Comportamento

| Cliques | Efeito |
|---------|--------|
| 1 | celula / arrastar |
| 2 | palavra (ja existia) |
| 3 | linha inteira (trim trailing empty) |

## Arquivos

- term.h/term.c: tswl_term_select_line (aditivo)
- main.c: click_count (OSC/word preservados)

Suite 30/30.
