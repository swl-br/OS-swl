# AVISO — T5: não compila + falta linha T3 (2026-09-09, orquestrador)

## Veredito: LÓGICA APROVADA (10/10 sanitizer), integração BLOQUEADA

term.c, highlight e clipboard conferidos e corretos. Mas:

## 1. Build quebrado (impeditivo)

`selection_copy` e `clipboard_paste` são usadas no handler de
teclado (~linhas 445/449) e definidas depois (~545/552), sem
declaração antecipada → erro duro no GCC (`static declaration follows
non-static`). Fix: 2 forward declarations antes do primeiro uso:

```c
static void selection_copy(struct app *a);
static void clipboard_paste(struct app *a);
```

## 2. Falta linha T3 no render.c

Igual ao t4-dirty: manter o `#define TSWL_FONT` com DejaVu (não
apagar T1).

Status: ATENDIDO em 2026-09-09 — pacote fix-t4-t5 traz forward decls; integrado.
