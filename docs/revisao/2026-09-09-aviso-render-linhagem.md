# AVISO — rebase render.c veio de outra linhagem (2026-09-09, orquestrador)

## Veredito: REJEITADO, refazer

O `render.c` do pacote `rebase-avisos-files/` não é rebase do atual:

1. Referencia `TSWL_MENUBAR_H` (e menubar) que **não existe** nesta
   árvore — nem compila aqui.
2. **Sem** a barra do cursor (T1) — reverteria o T1.
3. Acentos do PT-BR estragados nos comentários (dano de encoding —
   sinal de pipeline com conversão errada no meio do caminho).
4. Remove offsets `TSWL_RENDER_PAD` em pontos do desenho (mudança de
   layout não pedida).

## O que fazer

Refazer sobre o `render.c` atual (que tem T1 + linha T3), aplicando
SÓ a otimização dirty-row já aprovada (ver
`docs/revisao/2026-09-09-aviso-t4-dirty.md`, que continua PENDENTE).
Conferir encoding (UTF-8) antes de enviar.

Status: PENDENTE.
