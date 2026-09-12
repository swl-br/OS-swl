# AVISO — ctrl-alt-t: forward decl (2026-09-11, orquestrador)

## Veredito: LÓGICA APROVADA, integração BLOQUEADA (não compila)

Bloco Ctrl+Alt+T correto e base atual. Mas `swl_launch_command()`
é chamada (~751) antes de definida (~913), sem declaração —
erro duro (mesma classe dos bugs T5/osc52).

## O que falta

Forward declaration antes do primeiro uso:

```c
static void swl_launch_command(const char *cmd);
```

Status: PENDENTE.
