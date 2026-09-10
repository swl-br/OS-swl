# AVISO — t5-word: manter OSC (2026-09-10, orquestrador)

## Veredito: LÓGICA APROVADA, integração BLOQUEADA por base velha

`tswl_term_select_word` + duplo clique verificados e corretos. Mas os
arquivos vieram sem OSC (`take_title` decl/uso/captura) — subir apaga
o título da janela.

## O que fazer (sobre os arquivos atuais com OSC)

1. `term.c`: somar `select_word` (região disjunta do OSC).
2. `term.h`: somar decl (manter decl OSC).
3. `main.c`: somar lógica de duplo clique (manter bloco take_title).

Status: PENDENTE.
