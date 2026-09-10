# AVISO — T3: falta 1 linha no render.c (2026-09-09, orquestrador)

## Veredito: base (fonts.conf + fallback) APROVADA e integrada

`build-gui-rootfs.sh` com `fonts.conf` (alias monospace→DejaVu) e cópia
do host entrou. Diagnóstico da causa-raiz confirmado.

## O que falta (1 linha em `apps/tswl/src/render.c`)

O `render.c` entregue veio pré-T1 (cursor bloco); subir apagaria a
barra do T1. Rebase mínimo — trocar só:

```c
#define TSWL_FONT "JetBrains Mono, Fira Code, monospace"
```

por:

```c
/* T3: no rootfs mínimo do QEMU só costuma existir DejaVu*.
 * Sans Mono cobre U+2588; Sans é fallback se Mono não estiver instalada.
 * JetBrains/Fira ficam na frente no host de desenvolvimento. */
#define TSWL_FONT "JetBrains Mono, Fira Code, DejaVu Sans Mono, DejaVu Sans, monospace"
```

Nada mais do `render.c` da entrega deve entrar (o resto é código
pré-T1).

Status: ATENDIDO em 2026-09-09 — a linha entrou via commit 9edfe33ab (varredura do git add -A; conteúdo idêntico ao verificado). Aviso encerrado.
