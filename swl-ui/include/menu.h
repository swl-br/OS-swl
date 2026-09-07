#ifndef SWL_MENU_H
#define SWL_MENU_H

#include <stdbool.h>
#include <wlr/types/wlr_scene.h>

/*
 * Menu iniciar: lista vertical de apps, ancorada no canto inferior
 * esquerdo, logo acima da taskbar (mesma posição do botão MENU que já
 * dispara o toggle). Conteúdo vem do catálogo de apps já definido em
 * desktop.h — fonte única de verdade, a lista não é duplicada aqui.
 *
 * Fica escondido (nó da scene desabilitado) até ser aberto; nenhum
 * redesenho acontece em toggle, já que o conteúdo é estático — só
 * habilita/desabilita o nó e levanta pro topo da scene ao abrir.
 */
struct swl_menu;

struct swl_menu *swl_menu_create(struct wlr_scene_tree *parent);
void swl_menu_destroy(struct swl_menu *menu);

/* Reposiciona o menu (canto inferior esquerdo, acima da taskbar) quando o
 * output muda de tamanho — chamar com a mesma altura de tela que os
 * outros widgets (`server->screen_height`). */
void swl_menu_resize(struct swl_menu *menu, int screen_height);

bool swl_menu_is_open(struct swl_menu *menu);
void swl_menu_open(struct swl_menu *menu);
void swl_menu_close(struct swl_menu *menu);
void swl_menu_toggle(struct swl_menu *menu);

/* Resultado de um clique em coordenadas de layout (mesmo espaço de
 * server->cursor->x/y) enquanto o menu está aberto:
 *   >= 0                  -> índice do app clicado (linha do menu)
 *   SWL_MENU_HIT_INSIDE   -> clicou dentro do menu, fora de qualquer
 *                            linha (borda/padding) — absorve o clique
 *   SWL_MENU_HIT_OUTSIDE  -> clicou fora do retângulo do menu, ou o menu
 *                            está fechado
 */
#define SWL_MENU_HIT_INSIDE  (-1)
#define SWL_MENU_HIT_OUTSIDE (-2)
int swl_menu_hit_test(struct swl_menu *menu, double x, double y);

/* Comando (shell) do item de índice `index`, ou NULL se fora do range.
 * String interna — não precisa dar free. */
const char *swl_menu_item_command(struct swl_menu *menu, int index);

#endif /* SWL_MENU_H */
