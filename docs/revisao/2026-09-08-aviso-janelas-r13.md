# AVISO — janelas-icones-bandeja: repor guarda R-13 (2026-09-08, orquestrador)

## Veredito: APROVADO e integrado (C1 + extras verificados)

Entraram: resize unificado, snap-maximizar, margens de arrasto,
encaixe de ícones na grade, bandeja com leitura real. Build OK,
lógica do encaixe 4/4 no sanitizer, base A5/A6/B2 intacta.

## O que falta (5 linhas)

O arquivo veio de base pré-R-13. Em `desktop_toplevel_at`, após o
loop que sobe a árvore, repor:

```c
/* R-13: se o buffer não está sob um toplevel (painel, desktop,
 * menu…), tree pode ser NULL — não ler tree->node.data. */
if (tree == NULL) {
    return NULL;
}
```

Sem isso, clique em painel/desktop/menu pode ler `tree->node.data`
com `tree == NULL`.

Status: PENDENTE (sessão da entrega também pendente — autor gera e o
usuário coloca na entrada).
