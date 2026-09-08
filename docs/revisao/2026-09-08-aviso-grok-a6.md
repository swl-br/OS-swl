# AVISO ao Grok — A6 integrada com ressalva (2026-09-08, orquestrador)

## Veredito: APROVADA COM UM "MAS"

A lógica do fullscreen está boa (flag, layout, salva/restaura, exclusão
mútua com maximize, guards) — por isso a entrega foi integrada ao
`main`. Mas o arquivo veio montado sobre o `main` **pré-A5**, e com isso
um pedaço do A5 (já mergeado) ficou de fora. Falta repor.

## O que falta (item 1 — importante)

No `server_new_output`, ramo `else` (shell já existe), depois de
`swl_menu_resize(server->menu, oh);`, o `main` atual (`f1c4662`) tem
este bloco, que a entrega A6 não trouxe:

```c
/* Mesmo reflow de maximizadas que em output_request_state (A5). */
struct tinywl_toplevel *t;
wl_list_for_each(t, &server->toplevels, link) {
	if (t->maximized && !t->minimized) {
		toplevel_apply_maximized_layout(t);
	}
}
```

Sem ele, janela maximizada não acompanha nesse caminho (hotplug /
resize via output novo) — regressão parcial do A5. Favor repor o bloco
**estendido para fullscreen** (mesmo padrão do reflow que você já pôs
no `output_request_state`: pula `minimized`; `fullscreen` →
`toplevel_apply_fullscreen_layout`, `maximized` →
`toplevel_apply_maximized_layout`).

## Detalhe menor (item 2 — opcional)

Entrar em maximize com fullscreen ativo cancela o fullscreen mas não
traz painel/taskbar de volta pra cima (cura sozinha no próximo foco).
Se quiser lapidar: re-raise do shell nesse caminho, igual ao que você
fez na saída do fullscreen.

## Base para o complemento

Usar o `main` atual (pós `f1c4662`, que já contém A5 + este A6),
não o pré-A5.

Status: ATENDIDO em 2026-09-08 — complemento verificado (só os 2
itens, build 11/11) e integrado. A6 → CONCLUÍDA.
