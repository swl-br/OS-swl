# AVISO — t5-selectall: manter fix UTF-8 (2026-09-11, orquestrador)

## Veredito: LÓGICA APROVADA, integração BLOQUEADA por base velha

`select_all` + Ctrl+Shift+A corretos. Mas o arquivo veio sem o guard
UTF-8 (`utf8_left == 0`, commit 862539210) — subir apaga ele e o €/emoji
quebram de novo.

## O que fazer (sobre os arquivos atuais)

Somar SÓ `select_all` + decl + handler Ctrl+Shift+A, mantendo o guard.

Status: PENDENTE.
