# AVISO — w1-geom: faltam os campos buf_w/buf_h (2026-09-11, orquestrador)

## Veredito: LÓGICA APROVADA, integração BLOQUEADA (não compila)

Rastrear dims por slot + zerar buffer + mínimos: correto e mira na
causa provável dos farelos (reuso com tamanho errado). Mas o código
usa `b->buf_w` / `b->buf_h` sem declarar no `struct tswl_shm_buf` —
erro duro no GCC.

## O que falta

No struct (junto a `busy`/`stale`):

```c
int buf_w, buf_h;   /* dims da criação; pick só reusa se bate atual */
```

Status: ATENDIDO em 2026-09-11 — fix exato (campos + set + pick-guard + memset + mínimos) verificado com build limpo e integrado.
