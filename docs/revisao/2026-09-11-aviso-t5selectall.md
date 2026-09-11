# AVISO — selectall: `pressed` não existe (2026-09-11, orquestrador)

## Veredito: LÓGICA APROVADA, integração BLOQUEADA (não compila)

`select_all` + handler corretos, mas usa variável `pressed` que não
existe em `keyboard_key` (ela retorna cedo se não-pressed — linha 484;
vizinhos C/V nem checam). Erro duro no GCC.

## O que falta

Trocar `if (pressed && a->term)` por `if (a->term)` (o early return já
garante press).

Status: PENDENTE.
