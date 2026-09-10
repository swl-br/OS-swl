# AVISO — t4-dirty: manter linha de fonte do T3 (2026-09-09, orquestrador)

## Veredito: LÓGICA APROVADA, integração BLOQUEADA por base velha

Otimização dirty-row verificada em pixel (sem barra fantasma, blink
limpo, conteúdo preservado). Mas o `render.c` entregue não tem a linha
do T3 (lista DejaVu) — subir apagaria ela ( já houve esse problema
antes; ver aviso-t3-fontline).

## O que fazer

Sobre o `render.c` atual (que já tem T1 + linha T3), aplicar SÓ a
otimização dirty-row. Não trazer mais nada do arquivo entregue.

Status: PENDENTE.
