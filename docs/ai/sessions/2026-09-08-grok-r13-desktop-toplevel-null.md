# Sessão — 2026-09-08

IA: Grok
Data: 2026-09-08
Responsável: R-13 — guarda NULL em `desktop_toplevel_at`

## Escopo

Apenas `swl-ui/src/swlwm.c`.

Base: `origin/main` (`7849110`).

## Problema

O loop sobe a árvore de cena até achar `node.data` (toplevel). Se o
hit for em painel/desktop/menu, `tree` chega a NULL e o código lia
`tree->node.data` sem guarda. Hoje inalcançável na prática, mas barato
de proteger.

## Solução

```c
if (tree == NULL) {
    return NULL;
}
return tree->node.data;
```

## Arquivo

- `swl-ui/src/swlwm.c`
