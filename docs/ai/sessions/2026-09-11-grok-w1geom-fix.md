# Sessao — 2026-09-11

IA: Grok
Data: 2026-09-11
Responsavel: fix W1-geom (aviso-w1geom) — campos buf_w/buf_h

Base: origin/main (raw, com bell + select_all)

## Correcoes (main.c)

1. struct tswl_shm_buf: `int buf_w, buf_h`
2. shm_buf_create: seta buf_w/buf_h
3. pick_free_buf: so reusa se dims batem com a->width/height
4. redraw: memset buffer antes do copy (evita farelos)
5. configure: min size 160 / menubar+32, NULL check render, min 10x3

## Preservado

- selecting=false no ramo menubar
- select_line / click_count / select_all
- take_bell / bell_until_ms
- take_title
