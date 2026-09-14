#ifndef SWL_MENUBAR_H
#define SWL_MENUBAR_H

#include <stdbool.h>
#include <cairo/cairo.h>
#include "swl_theme.h"

/*
 * menubar: barra de menu por aplicativo (Arquivo, Editar, Ver…) — M2.
 *
 * Padrão visual novo (decidido em conjunto): a barra fica ESCONDIDA
 * por padrão. Uma seta no canto esquerdo fica sempre visível; clicar
 * nela abre a barra com uma animação de revelação (como se os títulos
 * saíssem de dentro da seta) e a seta gira. O campo de busca é um item
 * DENTRO da própria barra (não um elemento separado), alinhado à
 * direita; digitar nele mostra um dropdown de resultados por baixo.
 *
 * O app é dono do texto de busca e da lista de resultados (só ele sabe
 * indexar seu próprio conteúdo) — a lib só desenha e faz hit-test,
 * igual já fazia com os menus.
 */

typedef struct swl_menuitem {
    const char *label;
    int id;
    bool enabled;
} swl_menuitem;

typedef struct swl_menu {
    const char *title;
    const swl_menuitem *items;
    int item_count;
} swl_menu;

typedef struct { const char *label; const char *category; } swl_search_hit;

typedef struct swl_menubar swl_menubar;

#define SWL_MENUBAR_BAR_H 26

swl_menubar *swl_menubar_new(int width, const swl_menu *menus, int menu_count);
void swl_menubar_free(swl_menubar *mb);
void swl_menubar_resize(swl_menubar *mb, int width);

/* Avança a animação de abrir/fechar em dt_ms (chame a cada frame
 * enquanto swl_menubar_animating() for true). ~180ms de transição. */
void swl_menubar_tick(swl_menubar *mb, int dt_ms);
bool swl_menubar_animating(const swl_menubar *mb);
bool swl_menubar_is_expanded(const swl_menubar *mb);

/* O app chama isso todo frame ANTES de swl_menubar_draw pra atualizar
 * o texto de busca e os resultados (ambos só ponteiros, não copiados —
 * precisam continuar válidos até o próximo set_search ou draw).
 * hit_count = 0 esconde o dropdown de resultados. */
void swl_menubar_set_search(swl_menubar *mb, const char *text, bool focused,
                             const swl_search_hit *hits, int hit_count);

/* Retorna o índice do resultado sob (x,y), ou -1. */
int swl_menubar_search_hit_at(swl_menubar *mb, int x, int y);
/* Retângulo do campo de busca (pra saber onde por o cursor de texto,
 * desenhar foco, etc.) — só válido quando a barra está expandida. */
void swl_menubar_search_rect(swl_menubar *mb, int *x, int *y, int *w, int *h);

void swl_menubar_draw(const swl_theme_t *th, swl_menubar *mb, cairo_t *cr, int surface_w);

void swl_menubar_pointer_motion(swl_menubar *mb, int x, int y);

/* Clique. Retorna o id da ação (>0), ou 0 se não ativou item de menu —
 * inclui os casos de clicar na seta (abre/fecha) ou no campo de busca
 * (o app confere swl_menubar_search_rect/hit_at separadamente). */
int swl_menubar_pointer_button(swl_menubar *mb, int x, int y, bool pressed);

bool swl_menubar_is_open(swl_menubar *mb);

enum swl_menubar_key {
    SWL_MENUBAR_KEY_ALT,
    SWL_MENUBAR_KEY_LEFT,
    SWL_MENUBAR_KEY_RIGHT,
    SWL_MENUBAR_KEY_UP,
    SWL_MENUBAR_KEY_DOWN,
    SWL_MENUBAR_KEY_ENTER,
    SWL_MENUBAR_KEY_ESC,
};
int swl_menubar_key(swl_menubar *mb, enum swl_menubar_key key);

#endif /* SWL_MENUBAR_H */
