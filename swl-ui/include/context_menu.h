#ifndef SWL_CONTEXT_MENU_H
#define SWL_CONTEXT_MENU_H

#include <stdbool.h>
#include <wlr/types/wlr_scene.h>

/*
 * Menu de contexto genérico (botão direito do mouse) — diferente do menu
 * iniciar (menu.c), o conteúdo aqui é dinâmico: cada chamador passa a
 * lista de rótulos na hora de abrir, então o widget redesenha o buffer
 * a cada abertura em vez de desenhar uma vez só na criação.
 *
 * Uso típico: um `swl_context_menu` por compositor (não um por lugar
 * que pode abrir menu de contexto) — quem chama decide o conteúdo e o
 * índice do item clicado; a interpretação de "o que o item N faz" é
 * responsabilidade de quem abriu, não deste módulo.
 */
struct swl_context_menu;

#define SWL_CTXMENU_MAX_ITEMS 8

struct swl_context_menu *swl_context_menu_create(struct wlr_scene_tree *parent);
void swl_context_menu_destroy(struct swl_context_menu *menu);

/* Abre o menu ancorado em (x,y) — canto superior esquerdo do popup, em
 * coordenadas de layout (mesmo espaço de server->cursor->x/y). Ajusta
 * sozinho pra não vazar pra fora da tela (vira pra cima/esquerda se
 * necessário). `count` até SWL_CTXMENU_MAX_ITEMS; itens além disso são
 * ignorados. As strings em `labels` são copiadas — não precisam
 * continuar vivas depois desta chamada. */
void swl_context_menu_open(struct swl_context_menu *menu, int x, int y,
	int screen_width, int screen_height,
	const char *const *labels, int count);
void swl_context_menu_close(struct swl_context_menu *menu);
bool swl_context_menu_is_open(struct swl_context_menu *menu);

/* Mesma semântica do menu iniciar: índice >= 0 é o item clicado,
 * SWL_CTXMENU_HIT_INSIDE é clique dentro do popup fora de qualquer
 * linha, SWL_CTXMENU_HIT_OUTSIDE é fora do popup (ou menu fechado). */
#define SWL_CTXMENU_HIT_INSIDE  (-1)
#define SWL_CTXMENU_HIT_OUTSIDE (-2)
int swl_context_menu_hit_test(struct swl_context_menu *menu, double x, double y);

#endif /* SWL_CONTEXT_MENU_H */
